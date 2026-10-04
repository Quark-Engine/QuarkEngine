#ifndef __ENGINE_CPU_TASK_POOL_H__
#define __ENGINE_CPU_TASK_POOL_H__

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

struct SThreadUsageSnapshot
{
    std::string name;
    std::string currentTask;
    float utilizationPercent = 0.0f;
    std::vector<float> vHistory;
};

class CTaskPool
{
public:
    explicit CTaskPool(size_t workerCount = DefaultWorkerCount());
    ~CTaskPool();

    CTaskPool(const CTaskPool&) = delete;
    CTaskPool& operator=(const CTaskPool&) = delete;

    static size_t DefaultWorkerCount();
    static std::chrono::nanoseconds CurrentThreadCpuTime();
    void SetMainThreadTask(const std::string& taskName);
    void RecordMainThreadBusy(std::chrono::nanoseconds duration);
    void SampleUsage(std::chrono::nanoseconds interval);
    std::vector<SThreadUsageSnapshot> GetThreadUsageSnapshot() const;

    template<typename F>
    auto Submit(F&& function) -> std::future<std::invoke_result_t<F>>
    {
        return Submit("General CPU task", std::forward<F>(function));
    }

    template<typename F>
    auto Submit(const std::string& taskName, F&& function) -> std::future<std::invoke_result_t<F>>
    {
        using TResult = std::invoke_result_t<F>;
        auto pTask = std::make_shared<std::packaged_task<TResult()>>(
            std::forward<F>(function));
        std::future<TResult> future = pTask->get_future();
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Stopping)
            {
                throw std::runtime_error("Cannot submit a task to a stopped task pool");
            }
            m_Tasks.push_back({ taskName, [pTask]()
            {
                (*pTask)();
            } });
        }
        m_Condition.notify_one();
        return future;
    }

private:
    void WorkerLoop(size_t workerIndex);

    struct SThreadUsage
    {
        std::string name;
        std::string currentTask;
        std::chrono::nanoseconds accumulatedBusy{0};
        bool isBusy = false;
        std::deque<float> history;
    };

    struct SQueuedTask
    {
        std::string name;
        std::function<void()> execute;
    };

    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    std::deque<SQueuedTask> m_Tasks;
    std::vector<std::thread> m_vWorkers;
    bool m_Stopping = false;

    mutable std::mutex m_UsageMutex;
    std::vector<SThreadUsage> m_vThreadUsage;
};

#endif // __ENGINE_CPU_TASK_POOL_H__
