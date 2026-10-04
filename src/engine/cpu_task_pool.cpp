#include "engine/cpu_task_pool.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <time.h>
#endif

CTaskPool::CTaskPool(size_t workerCount)
{
    workerCount = std::max<size_t>(workerCount, 1);
    m_vWorkers.reserve(workerCount);
    m_vThreadUsage.reserve(workerCount + 1);
    m_vThreadUsage.push_back({
        "Main thread: editor, scene, rendering and GPU resource uploads"
    });
    for (size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
    {
        m_vThreadUsage.push_back({
            "CPU task worker " + std::to_string(workerIndex + 1)
        });
    }

    try
    {
        for (size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
        {
            m_vWorkers.emplace_back([this, workerIndex]()
            {
                WorkerLoop(workerIndex);
            });
        }
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Stopping = true;
        }
        m_Condition.notify_all();
        for (std::thread& worker : m_vWorkers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        throw;
    }
}

CTaskPool::~CTaskPool()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stopping = true;
    }
    m_Condition.notify_all();
    for (std::thread& worker : m_vWorkers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

size_t CTaskPool::DefaultWorkerCount()
{
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads <= 1)
    {
        return 1;
    }
    return std::min<size_t>(hardwareThreads - 1, 4);
}

std::chrono::nanoseconds CTaskPool::CurrentThreadCpuTime()
{
#ifdef _WIN32
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (!GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime))
    {
        throw std::runtime_error("Failed to read current thread CPU time");
    }

    ULARGE_INTEGER kernelTicks{};
    ULARGE_INTEGER userTicks{};
    kernelTicks.LowPart = kernelTime.dwLowDateTime;
    kernelTicks.HighPart = kernelTime.dwHighDateTime;
    userTicks.LowPart = userTime.dwLowDateTime;
    userTicks.HighPart = userTime.dwHighDateTime;
    constexpr std::uint64_t HUNDRED_NS_PER_NS = 100;
    const std::uint64_t totalTicks = kernelTicks.QuadPart + userTicks.QuadPart;
    return std::chrono::nanoseconds(totalTicks * HUNDRED_NS_PER_NS);
#else
    timespec threadTime{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &threadTime) != 0)
    {
        throw std::runtime_error("Failed to read current thread CPU time");
    }
    return std::chrono::seconds(threadTime.tv_sec) +
        std::chrono::nanoseconds(threadTime.tv_nsec);
#endif
}

void CTaskPool::SetMainThreadTask(const std::string& taskName)
{
    std::lock_guard<std::mutex> lock(m_UsageMutex);
    m_vThreadUsage.front().currentTask = taskName;
}

void CTaskPool::RecordMainThreadBusy(std::chrono::nanoseconds duration)
{
    if (duration <= std::chrono::nanoseconds::zero())
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_UsageMutex);
    m_vThreadUsage.front().accumulatedBusy += duration;
}

void CTaskPool::SampleUsage(std::chrono::nanoseconds interval)
{
    if (interval <= std::chrono::nanoseconds::zero())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_UsageMutex);
    for (SThreadUsage& thread : m_vThreadUsage)
    {
        const double ratio = static_cast<double>(thread.accumulatedBusy.count()) /
            static_cast<double>(interval.count());
        thread.history.push_back(static_cast<float>(std::clamp(ratio * 100.0, 0.0, 100.0)));
        if (thread.history.size() > 120)
        {
            thread.history.pop_front();
        }
        thread.accumulatedBusy = std::chrono::nanoseconds::zero();
    }
}

std::vector<SThreadUsageSnapshot> CTaskPool::GetThreadUsageSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_UsageMutex);
    std::vector<SThreadUsageSnapshot> vSnapshot;
    vSnapshot.reserve(m_vThreadUsage.size());
    for (const SThreadUsage& thread : m_vThreadUsage)
    {
        SThreadUsageSnapshot snapshot;
        snapshot.name = thread.name;
        snapshot.currentTask = thread.currentTask;
        snapshot.utilizationPercent = thread.history.empty() ? 0.0f : thread.history.back();
        snapshot.vHistory.assign(thread.history.begin(), thread.history.end());
        vSnapshot.push_back(std::move(snapshot));
    }
    return vSnapshot;
}

void CTaskPool::WorkerLoop(size_t workerIndex)
{
    while (true)
    {
        SQueuedTask task;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Condition.wait(lock, [this]()
            {
                return m_Stopping || !m_Tasks.empty();
            });
            if (m_Stopping && m_Tasks.empty())
            {
                return;
            }
            task = std::move(m_Tasks.front());
            m_Tasks.pop_front();
        }

        const std::chrono::nanoseconds taskStart = CurrentThreadCpuTime();
        {
            std::lock_guard<std::mutex> lock(m_UsageMutex);
            SThreadUsage& thread = m_vThreadUsage[workerIndex + 1];
            thread.currentTask = task.name;
            thread.isBusy = true;
        }
        task.execute();
        const std::chrono::nanoseconds taskEnd = CurrentThreadCpuTime();
        {
            std::lock_guard<std::mutex> lock(m_UsageMutex);
            SThreadUsage& thread = m_vThreadUsage[workerIndex + 1];
            thread.accumulatedBusy += taskEnd - taskStart;
            thread.isBusy = false;
            thread.currentTask.clear();
        }
    }
}
