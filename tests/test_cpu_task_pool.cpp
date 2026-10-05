#include "test_harness.h"

#include "editable_mesh.h"
#include "engine/cpu_task_pool.h"

#include <chrono>
#include <atomic>
#include <future>
#include <stdexcept>
#include <thread>

TEST(CpuTaskPool, ReturnsResultsAndPropagatesExceptions)
{
    CTaskPool taskPool(2);
    auto first = taskPool.Submit([]()
    {
        return 21 * 2;
    });
    auto second = taskPool.Submit([]() -> int
    {
        throw std::runtime_error("task failure");
    });

    CHECK(first.get() == 42);
    bool exceptionPropagated = false;
    try
    {
        second.get();
    }
    catch (const std::runtime_error&)
    {
        exceptionPropagated = true;
    }
    CHECK(exceptionPropagated);
}

TEST(CpuTaskPool, BuildsEditableGeometryFromOwnedCopy)
{
    CTaskPool taskPool(2);
    CEditableMesh editableMesh;
    editableMesh.m_vVertices = {
        { { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f },
        { { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f },
        { { 0.0f, 1.0f, 0.0f }, 0.0f, 1.0f }
    };
    editableMesh.m_vTriangles = { { 0, 1, 2 } };

    auto geometry = taskPool.Submit([mesh = std::move(editableMesh)]()
    {
        return BuildEditableMeshData(mesh);
    });
    const SEditableMeshBuildData buildData = geometry.get();

    CHECK(buildData.IsValid);
    CHECK(buildData.vVertices.size() == 9);
    CHECK(buildData.vNormals.size() == 9);
    CHECK(buildData.vTexcoords.size() == 6);
    CHECK(buildData.vIndices.size() == 3);
    CHECK(buildData.vIndices[0] == 0);
    CHECK(buildData.vIndices[1] == 1);
    CHECK(buildData.vIndices[2] == 2);
    CHECK_NEAR(buildData.vNormals[2], 1.0f, 1e-6);
}

TEST(CpuTaskPool, CapturesNamedThreadUsageHistory)
{
    CTaskPool taskPool(1);
    taskPool.RecordMainThreadBusy(std::chrono::milliseconds(5));
    std::promise<void> started;
    std::future<void> startedFuture = started.get_future();
    auto task = taskPool.Submit("Decode test image", [&started]()
    {
        started.set_value();
        const auto workEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
        std::atomic<unsigned int> operations{0};
        while (std::chrono::steady_clock::now() < workEnd)
        {
            operations.fetch_add(1, std::memory_order_relaxed);
        }
    });
    startedFuture.wait();
    const std::vector<SThreadUsageSnapshot> vActiveUsage = taskPool.GetThreadUsageSnapshot();
    CHECK(vActiveUsage[1].currentTask == "Decode test image");
    task.get();

    const auto idleDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    std::vector<SThreadUsageSnapshot> vUsage;
    do
    {
        vUsage = taskPool.GetThreadUsageSnapshot();
        if (vUsage[1].currentTask.empty())
        {
            break;
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < idleDeadline);

    taskPool.SampleUsage(std::chrono::milliseconds(50));
    vUsage = taskPool.GetThreadUsageSnapshot();
    CHECK(vUsage.size() == 2);
    CHECK(vUsage[0].name.find("GPU resource uploads") != std::string::npos);
    CHECK(vUsage[0].vHistory.size() == 1);
    CHECK_NEAR(vUsage[0].utilizationPercent, 0.645f, 0.02f);
    CHECK(vUsage[1].name == "CPU task worker 1");
    CHECK(vUsage[1].vHistory.size() == 1);
    CHECK(vUsage[1].utilizationPercent > 0.0f);
    CHECK(vUsage[1].currentTask.empty());

    taskPool.SampleUsage(std::chrono::milliseconds(50));
    const std::vector<SThreadUsageSnapshot> vSmoothedUsage =
        taskPool.GetThreadUsageSnapshot();
    CHECK(vSmoothedUsage[0].vHistory.size() == 2);
    CHECK(vSmoothedUsage[0].vHistory[1] < vSmoothedUsage[0].vHistory[0]);
    CHECK(vSmoothedUsage[0].vHistory[1] > 0.0f);
}

TEST(CpuTaskPool, RejectsInvalidEditableGeometry)
{
    CEditableMesh editableMesh;
    editableMesh.m_vVertices.push_back({ { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f });
    editableMesh.m_vTriangles.push_back({ 0, 1, 0 });

    CHECK(!BuildEditableMeshData(editableMesh).IsValid);
}
