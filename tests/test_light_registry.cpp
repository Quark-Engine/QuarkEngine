#include "test_harness.h"

#include "lighting.h"

namespace
{

std::vector<int> AllocateAll(CLightRegistry& registry)
{
    std::vector<int> vIds;
    for (int i = 0; i < QC_MAX_LIGHTS; i++)
    {
        vIds.push_back(registry.Allocate());
    }
    return vIds;
}

} // anonymous

TEST(LightRegistry, allocates_sequential_ids)
{
    CLightRegistry registry;

    CHECK(registry.AllocatedCount() == 0);
    CHECK(registry.Allocate() == 0);
    CHECK(registry.Allocate() == 1);
    CHECK(registry.Allocate() == 2);
    CHECK(registry.AllocatedCount() == 3);
}

TEST(LightRegistry, exhausted_registry_returns_invalid_id)
{
    CLightRegistry registry;
    const std::vector<int> vIds = AllocateAll(registry);

    CHECK(static_cast<int>(vIds.size()) == QC_MAX_LIGHTS);
    CHECK(registry.AllocatedCount() == QC_MAX_LIGHTS);

    for (int id : vIds)
    {
        CHECK(id != CLightRegistry::INVALID_ID);
        CHECK(registry.IsAllocated(id));
    }

    CHECK(registry.Allocate() == CLightRegistry::INVALID_ID);
    CHECK(registry.AllocatedCount() == QC_MAX_LIGHTS);
}

TEST(LightRegistry, free_returns_the_lowest_free_id_to_the_pool)
{
    CLightRegistry registry;
    const std::vector<int> vIds = AllocateAll(registry);

    registry.Free(vIds[3]);
    CHECK(!registry.IsAllocated(vIds[3]));
    CHECK(registry.AllocatedCount() == QC_MAX_LIGHTS - 1);
    CHECK(registry.Allocate() == vIds[3]);
    CHECK(registry.AllocatedCount() == QC_MAX_LIGHTS);
}

TEST(LightRegistry, free_does_not_release_an_id_twice)
{
    CLightRegistry registry;

    CHECK(registry.Allocate() == 0);
    CHECK(registry.Allocate() == 1);

    registry.Free(0);
    registry.Free(0);
    CHECK(registry.AllocatedCount() == 1);
    CHECK(registry.Allocate() == 0);
}

TEST(LightRegistry, free_ignores_out_of_range_ids)
{
    CLightRegistry registry;
    CHECK(registry.Allocate() == 0);

    registry.Free(CLightRegistry::INVALID_ID);
    registry.Free(-42);
    registry.Free(QC_MAX_LIGHTS);
    registry.Free(QC_MAX_LIGHTS + 1000);

    CHECK(registry.AllocatedCount() == 1);
    CHECK(!registry.IsAllocated(CLightRegistry::INVALID_ID));
    CHECK(!registry.IsAllocated(-42));
    CHECK(!registry.IsAllocated(QC_MAX_LIGHTS));
    CHECK(!registry.IsAllocated(QC_MAX_LIGHTS + 1000));
}

TEST(LightRegistry, reset_releases_every_slot)
{
    CLightRegistry registry;
    AllocateAll(registry);
    CHECK(registry.AllocatedCount() == QC_MAX_LIGHTS);

    registry.Reset();

    CHECK(registry.AllocatedCount() == 0);
    CHECK(!registry.IsAllocated(0));
    CHECK(!registry.IsAllocated(QC_MAX_LIGHTS - 1));
    CHECK(registry.Allocate() == 0);
}

TEST(LightRegistry, instances_do_not_share_state)
{
    CLightRegistry first;
    CLightRegistry second;

    CHECK(first.Allocate() == 0);
    CHECK(first.Allocate() == 1);

    CHECK(second.AllocatedCount() == 0);
    CHECK(second.Allocate() == 0);

    first.Reset();
    CHECK(second.AllocatedCount() == 1);
}
