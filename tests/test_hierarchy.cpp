#include "test_harness.h"

#include "editor/editor_hierarchy_utils.h"
#include "engine/transform.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace qc;

namespace
{

void ExpectIndices(const std::vector<int>& vActual, const std::vector<int>& vExpected)
{
    std::vector<int> vSortedActual = vActual;
    std::vector<int> vSortedExpected = vExpected;
    std::sort(vSortedActual.begin(), vSortedActual.end());
    std::sort(vSortedExpected.begin(), vSortedExpected.end());
    CHECK(vSortedActual == vSortedExpected);
}

int Add(CScene& target, const std::string& name, int parentId, bool isGroup = false)
{
    CEntity added(static_cast<int>(target.m_vEntities.size()));
    added.m_Name = name;
    added.m_ParentId = parentId;
    added.m_IsGroup = isGroup;
    target.m_vEntities.push_back(added);
    return static_cast<int>(target.m_vEntities.size()) - 1;
}

} // anonymous

TEST(GetEntityChildren, finds_direct_children_only)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "root", -1);
    Add(sceneUnderTest, "a", 0);
    Add(sceneUnderTest, "b", 0);
    Add(sceneUnderTest, "a1", 1);

    ExpectIndices(GetEntityChildren(sceneUnderTest, 0), { 1, 2 });
    ExpectIndices(GetEntityChildren(sceneUnderTest, 1), { 3 });
    ExpectIndices(GetEntityChildren(sceneUnderTest, 3), {});
}

TEST(GetEntityChildren, roots_are_children_of_minus_one)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "a", -1);
    Add(sceneUnderTest, "b", -1);
    Add(sceneUnderTest, "child", 0);

    ExpectIndices(GetRootEntities(sceneUnderTest), { 0, 1 });
}

TEST(GetEntityDescendants, walks_the_whole_subtree)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "root", -1);
    Add(sceneUnderTest, "a", 0);
    Add(sceneUnderTest, "b", 0);
    Add(sceneUnderTest, "a1", 1);
    Add(sceneUnderTest, "a1x", 3);
    Add(sceneUnderTest, "unrelated", -1);

    ExpectIndices(GetEntityDescendants(sceneUnderTest, 0), { 1, 2, 3, 4 });
}

TEST(GetEntityDescendants, leaf_has_none)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "root", -1);
    Add(sceneUnderTest, "child", 0);

    CHECK(GetEntityDescendants(sceneUnderTest, 1).empty());
}

TEST(IsEntityGroup, reflects_the_flag)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "plain", -1);
    Add(sceneUnderTest, "group", -1, true);

    CHECK(!IsEntityGroup(sceneUnderTest.m_vEntities[0]));
    CHECK(IsEntityGroup(sceneUnderTest.m_vEntities[1]));
}

TEST(CreateGroup, appends_a_group_with_a_unique_id)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "existing", -1);

    const int id = CreateGroup(sceneUnderTest, "Group");
    CHECK(id == 1);
    CHECK(static_cast<int>(sceneUnderTest.m_vEntities.size()) == 2);
    CHECK(sceneUnderTest.m_vEntities[id].m_IsGroup);
    CHECK(sceneUnderTest.m_vEntities[id].m_Name == "Group");
    CHECK(sceneUnderTest.m_vEntities[id].m_ParentId == -1);
}

TEST(CreateGroup, can_nest_under_an_existing_entity)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "parent", -1);

    const int id = CreateGroup(sceneUnderTest, "Nested", 0);
    CHECK(sceneUnderTest.m_vEntities[id].m_ParentId == 0);
    ExpectIndices(GetEntityChildren(sceneUnderTest, 0), { 1 });
}

TEST(MoveEntityToParent, rejects_invalid_arguments)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "a", -1);

    MoveEntityToParent(sceneUnderTest, -1, 0);
    MoveEntityToParent(sceneUnderTest, 99, -1);
    MoveEntityToParent(sceneUnderTest, 0, 0);

    CHECK(sceneUnderTest.m_vEntities[0].m_ParentId == -1);
}

TEST(MoveEntityToParent, refuses_to_parent_an_entity_to_its_own_descendant)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "a", -1);
    Add(sceneUnderTest, "b", 0);
    Add(sceneUnderTest, "c", 1);

    MoveEntityToParent(sceneUnderTest, 0, 2);

    CHECK_MSG(sceneUnderTest.m_vEntities[0].m_ParentId == -1,
        "a must not become a child of c, that would build a cycle");
}

TEST(MoveEntityToParent, keeps_the_world_position_when_reparenting)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "parent", -1);
    Add(sceneUnderTest, "child", -1);

    sceneUnderTest.m_vEntities[0].GetTransformComponent()->m_Position = { 10.0f, 0.0f, 0.0f };
    sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Position = { 3.0f, 4.0f, 5.0f };

    MoveEntityToParent(sceneUnderTest, 1, 0);

    CHECK(sceneUnderTest.m_vEntities[1].m_ParentId == 0);
    CHECK_NEAR(sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Position.x, -7.0f, 1e-3);
    CHECK_NEAR(sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Position.y, 4.0f, 1e-3);
    CHECK_NEAR(sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Position.z, 5.0f, 1e-3);
}

TEST(MoveEntityToParent, preserves_the_world_transform_with_a_rotated_non_uniform_parent)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "parent", -1);
    Add(sceneUnderTest, "child", -1);

    CTransformComponent* pParentTransform = sceneUnderTest.m_vEntities[0].GetTransformComponent();
    pParentTransform->m_Position = { 5.0f, -3.0f, 2.0f };
    pParentTransform->m_Rotation = { 20.0f, 35.0f, -15.0f };
    pParentTransform->m_Scale = { 2.0f, 0.5f, 3.0f };

    CTransformComponent* pChildTransform = sceneUnderTest.m_vEntities[1].GetTransformComponent();
    pChildTransform->m_Position = { -4.0f, 6.0f, 1.0f };
    pChildTransform->m_Rotation = { -25.0f, 40.0f, 10.0f };
    pChildTransform->m_Scale = { 0.75f, 1.5f, 2.0f };

    const Mat4 worldBefore = quark::ComposeWorld(sceneUnderTest, 1);
    MoveEntityToParent(sceneUnderTest, 1, 0);
    const Mat4 worldAfter = quark::ComposeWorld(sceneUnderTest, 1);

    CHECK(sceneUnderTest.m_vEntities[1].m_ParentId == 0);
    for (int index = 0; index < 16; ++index)
    {
        CHECK_NEAR(worldAfter.m[index], worldBefore.m[index], 1e-4);
    }
    CHECK(pChildTransform->m_HasLocalMatrixOverride);
}

TEST(DeleteGroup, reparents_children_to_the_grandparent_by_default)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "root", -1);
    Add(sceneUnderTest, "group", 0, true);
    Add(sceneUnderTest, "child", 1);

    DeleteGroup(sceneUnderTest, 1);

    CHECK(static_cast<int>(sceneUnderTest.m_vEntities.size()) == 2);
    CHECK(sceneUnderTest.m_vEntities[1].m_Name == "child");
    CHECK_MSG(sceneUnderTest.m_vEntities[1].m_ParentId == 0,
        "child must point at the erased group's parent");
}

TEST(DeleteGroup, preserves_child_world_transform_under_a_non_uniform_parent)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "root", -1);
    Add(sceneUnderTest, "group", 0, true);
    Add(sceneUnderTest, "child", 1);
    Add(sceneUnderTest, "later-parent", -1);
    Add(sceneUnderTest, "later-child", 3);

    sceneUnderTest.m_vEntities[0].GetTransformComponent()->m_Rotation = { 15.0f, 25.0f, 5.0f };
    sceneUnderTest.m_vEntities[0].GetTransformComponent()->m_Scale = { 2.0f, 0.5f, 3.0f };
    sceneUnderTest.m_vEntities[2].GetTransformComponent()->m_Position = { 3.0f, -1.0f, 4.0f };
    sceneUnderTest.m_vEntities[2].GetTransformComponent()->m_Rotation = { 20.0f, 30.0f, -10.0f };
    sceneUnderTest.m_vEntities[3].GetTransformComponent()->m_Position = { -4.0f, 2.0f, 1.0f };

    const Mat4 worldBefore = quark::ComposeWorld(sceneUnderTest, 2);
    DeleteGroup(sceneUnderTest, 1);

    CHECK(sceneUnderTest.m_vEntities[1].m_ParentId == 0);
    CHECK(sceneUnderTest.m_vEntities[2].m_Id == 2);
    CHECK(sceneUnderTest.m_vEntities[2].m_ParentId == -1);
    CHECK(sceneUnderTest.m_vEntities[3].m_Id == 3);
    CHECK(sceneUnderTest.m_vEntities[3].m_ParentId == 2);
    const Mat4 worldAfter = quark::ComposeWorld(sceneUnderTest, 1);
    for (int index = 0; index < 16; ++index)
    {
        CHECK_NEAR(worldAfter.m[index], worldBefore.m[index], 1e-4);
    }
}

TEST(DeleteGroup, refuses_to_delete_a_plain_entity)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "plain", -1);

    DeleteGroup(sceneUnderTest, 0);

    CHECK(static_cast<int>(sceneUnderTest.m_vEntities.size()) == 1);
}

TEST(DeleteGroup, ignores_invalid_indices)
{
    CScene sceneUnderTest;
    Add(sceneUnderTest, "plain", -1);

    DeleteGroup(sceneUnderTest, -1);
    DeleteGroup(sceneUnderTest, 99);

    CHECK(static_cast<int>(sceneUnderTest.m_vEntities.size()) == 1);
}
