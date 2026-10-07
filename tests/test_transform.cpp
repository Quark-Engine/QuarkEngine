#include "test_harness.h"

#include "engine/transform.h"

#include "nlohmann/json.hpp"

#include "QuarkCore/QuarkCore.hpp"

#include <vector>

namespace
{

void SetTransform(CEntity& target, Vec3 position, Vec3 rotationDegrees, Vec3 scale)
{
    CTransformComponent* pTransform = target.GetTransformComponent();
    pTransform->m_Position = position;
    pTransform->m_Rotation = rotationDegrees;
    pTransform->m_Scale = scale;
}

void ExpectMatNear(const Mat4& actual, const Mat4& expected, double eps)
{
    for (int i = 0; i < 16; ++i)
    {
        CHECK_NEAR(actual.m[i], expected.m[i], eps);
    }
}

void ExpectVecNear(const Vec3& actual, const Vec3& expected, double eps)
{
    CHECK_NEAR(actual.x, expected.x, eps);
    CHECK_NEAR(actual.y, expected.y, eps);
    CHECK_NEAR(actual.z, expected.z, eps);
}

int PushEntity(CScene& target, Vec3 position, Vec3 rotationDegrees, Vec3 scale, int parentId)
{
    CEntity added(static_cast<int>(target.m_vEntities.size()));
    SetTransform(added, position, rotationDegrees, scale);
    added.m_ParentId = parentId;
    target.m_vEntities.push_back(added);
    return static_cast<int>(target.m_vEntities.size()) - 1;
}

Mat4 LegacyRaylibStyleCompose(const CTransformComponent& transform)
{
    Mat4 matScale = Mat4Scale(transform.m_Scale.x, transform.m_Scale.y, transform.m_Scale.z);
    Mat4 matRotation = Mat4RotateXYZ(
    {
        transform.m_Rotation.x * DEG2RAD,
        transform.m_Rotation.y * DEG2RAD,
        transform.m_Rotation.z * DEG2RAD
    });
    Mat4 matTranslation = Mat4Translate(
        transform.m_Position.x, transform.m_Position.y, transform.m_Position.z);
    return Mat4Multiply(Mat4Multiply(matTranslation, matRotation), matScale);
}

} // anonymous

TEST(ComposeLocal, matches_the_raylib_style_implementation_it_replaced)
{
    CTransformComponent transform;
    transform.m_Position = { 3.0f, -2.5f, 7.25f };
    transform.m_Rotation = { 35.0f, -110.0f, 22.5f };
    transform.m_Scale = { 2.0f, 0.5f, 1.75f };

    ExpectMatNear(quark::ComposeLocal(transform), LegacyRaylibStyleCompose(transform), 1e-6);
}

TEST(ComposeLocal, identity_transform_is_identity)
{
    CTransformComponent transform;
    ExpectMatNear(quark::ComposeLocal(transform), Mat4::identity(), 1e-6);
}

TEST(ComposeLocal, translation_lands_in_the_last_column)
{
    CTransformComponent transform;
    transform.m_Position = { 10.0f, 20.0f, 30.0f };

    const Mat4 result = quark::ComposeLocal(transform);
    CHECK_NEAR(result.m[12], 10.0f, 1e-5);
    CHECK_NEAR(result.m[13], 20.0f, 1e-5);
    CHECK_NEAR(result.m[14], 30.0f, 1e-5);
}

TEST(ComposeLocal, entity_overload_reads_the_transform_component)
{
    CEntity target;
    SetTransform(target, { 1.0f, 2.0f, 3.0f }, { 0.0f, 90.0f, 0.0f }, { 2.0f, 2.0f, 2.0f });

    const CTransformComponent* pTransform = target.GetTransformComponent();
    ExpectMatNear(quark::ComposeLocal(target), quark::ComposeLocal(*pTransform), 1e-6);
}

TEST(ComposeLocal, local_matrix_override_survives_serialization)
{
    CTransformComponent transform;
    Mat4 local = Mat4::translation(3.0f, -2.0f, 7.0f) *
        Mat4::rotationY(35.0f * DEG2RAD) *
        Mat4::scale(2.0f, 0.5f, 3.0f);
    local.m[1] += 0.25f;
    transform.SetLocalMatrixOverride(local);

    nlohmann::json document;
    transform.Serialize(document);
    CTransformComponent restored;
    restored.Deserialize(document);

    CHECK(restored.m_HasLocalMatrixOverride);
    ExpectMatNear(quark::ComposeLocal(restored), local, 1e-6);
}

TEST(ComposeWorld, root_entity_world_equals_its_local_transform)
{
    CScene sceneUnderTest;
    const int index = PushEntity(sceneUnderTest, { 4.0f, 5.0f, 6.0f }, { 0.0f, 45.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, -1);

    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, index),
        quark::ComposeLocal(sceneUnderTest.m_vEntities[index]), 1e-6);
}

TEST(ComposeWorld, child_world_is_parent_world_times_child_local)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 10.0f, 0.0f, 0.0f }, { 0.0f, 90.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);
    const int child = PushEntity(sceneUnderTest, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 0);

    const Mat4 expected = quark::ComposeWorld(sceneUnderTest, 0)
        * quark::ComposeLocal(sceneUnderTest.m_vEntities[child]);
    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, child), expected, 1e-5);
}

TEST(ComposeWorld, multi_level_chain_accumulates_every_ancestor)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, -1);
    PushEntity(sceneUnderTest, { 0.0f, 2.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 0);
    const int leaf = PushEntity(sceneUnderTest, { 0.0f, 0.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 1);

    const Vec3 origin = quark::ComposeWorld(sceneUnderTest, leaf) * Vec3{ 0.0f, 0.0f, 0.0f };
    ExpectVecNear(origin, Vec3{ 1.0f, 2.0f, 3.0f }, 1e-4);
}

TEST(ComposeWorld, out_of_range_index_returns_identity)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 1.0f, 1.0f, 1.0f }, { 10.0f, 20.0f, 30.0f }, { 2.0f, 2.0f, 2.0f }, -1);

    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, -1), Mat4::identity(), 1e-6);
    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, 99), Mat4::identity(), 1e-6);
}

TEST(ComposeWorld, self_parent_is_treated_as_a_root)
{
    CScene sceneUnderTest;
    const int index = PushEntity(sceneUnderTest, { 2.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 3.0f, 3.0f, 3.0f }, 0);

    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, index),
        quark::ComposeLocal(sceneUnderTest.m_vEntities[index]), 1e-6);
}

TEST(ComposeWorld, two_node_cycle_terminates_with_a_finite_matrix)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);
    PushEntity(sceneUnderTest, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, 1);
    sceneUnderTest.m_vEntities[0].m_ParentId = 1;  // 0 -> 1 -> 0

    const Mat4 result = quark::ComposeWorld(sceneUnderTest, 1);
    for (int i = 0; i < 16; ++i)
    {
        CHECK(std::isfinite(result.m[i]));
    }
}

TEST(IndexOfEntity, finds_entities_owned_by_the_scene)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, {}, {}, { 1, 1, 1 }, -1);
    PushEntity(sceneUnderTest, {}, {}, { 1, 1, 1 }, -1);

    CHECK(quark::IndexOfEntity(sceneUnderTest, sceneUnderTest.m_vEntities[0]) == 0);
    CHECK(quark::IndexOfEntity(sceneUnderTest, sceneUnderTest.m_vEntities[1]) == 1);
}

TEST(IndexOfEntity, returns_minus_one_for_a_foreign_entity)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, {}, {}, { 1, 1, 1 }, -1);

    CEntity foreign;
    CHECK(quark::IndexOfEntity(sceneUnderTest, foreign) == -1);
}

TEST(ComposeWorld, foreign_entity_falls_back_to_its_local_transform)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 9.0f, 9.0f, 9.0f }, { 45.0f, 45.0f, 45.0f }, { 3.0f, 3.0f, 3.0f }, -1);

    CEntity foreign;
    SetTransform(foreign, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });

    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, foreign),
        quark::ComposeLocal(foreign), 1e-6);
}

TEST(ComposeWorld, entity_overload_matches_index_overload)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 3.0f, 1.0f, 0.0f }, { 0.0f, 30.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, -1);
    const int child = PushEntity(sceneUnderTest, { 0.0f, 2.0f, 0.0f }, { 15.0f, 0.0f, 0.0f }, { 1.5f, 1.5f, 1.5f }, 0);

    ExpectMatNear(quark::ComposeWorld(sceneUnderTest, sceneUnderTest.m_vEntities[child]),
        quark::ComposeWorld(sceneUnderTest, child), 1e-6);
}

TEST(ComposeMeshWorld, multiplies_by_the_model_local_matrix)
{
    CScene sceneUnderTest;
    const int index = PushEntity(sceneUnderTest, { 2.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, -1);

    CMeshComponent* pMesh = sceneUnderTest.m_vEntities[index].GetMeshComponent();
    pMesh->m_Model.transform = Mat4::translation(5.0f, 0.0f, 0.0f);

    const Mat4 expected = quark::ComposeWorld(sceneUnderTest, index) * pMesh->m_Model.transform;
    ExpectMatNear(quark::ComposeMeshWorld(sceneUnderTest, sceneUnderTest.m_vEntities[index]), expected, 1e-5);

    const Vec3 origin = quark::ComposeMeshWorld(sceneUnderTest, sceneUnderTest.m_vEntities[index])
        * Vec3{ 0.0f, 0.0f, 0.0f };
    ExpectVecNear(origin, Vec3{ 7.0f, 0.0f, 0.0f }, 1e-4);
}

TEST(DecomposeWorld, round_trips_position)
{
    CTransformComponent source;
    source.m_Position = { 12.5f, -3.25f, 8.0f };
    source.m_Scale = { 1.0f, 1.0f, 1.0f };

    CTransformComponent result;
    quark::DecomposeWorld(quark::ComposeLocal(source), result);

    ExpectVecNear(result.m_Position, source.m_Position, 1e-3);
}

TEST(DecomposeWorld, round_trips_scale)
{
    CTransformComponent source;
    source.m_Scale = { 2.5f, 0.75f, 4.0f };

    CTransformComponent result;
    quark::DecomposeWorld(quark::ComposeLocal(source), result);

    ExpectVecNear(result.m_Scale, source.m_Scale, 1e-3);
}

TEST(DecomposeWorld, round_trips_rotation)
{
    CTransformComponent source;
    source.m_Rotation = { 20.0f, 35.0f, -50.0f };

    CTransformComponent result;
    quark::DecomposeWorld(quark::ComposeLocal(source), result);

    ExpectVecNear(result.m_Rotation, source.m_Rotation, 0.5);
}

TEST(DecomposeWorld, round_trips_position_rotation_and_scale_together)
{
    CTransformComponent source;
    source.m_Position = { 3.0f, -2.0f, 5.0f };
    source.m_Rotation = { 10.0f, 25.0f, 40.0f };
    source.m_Scale = { 1.5f, 2.0f, 0.5f };

    CTransformComponent result;
    quark::DecomposeWorld(quark::ComposeLocal(source), result);

    ExpectVecNear(result.m_Position, source.m_Position, 1e-3);
    ExpectVecNear(result.m_Rotation, source.m_Rotation, 0.5);
    ExpectVecNear(result.m_Scale, source.m_Scale, 1e-3);
}

TEST(DecomposeLocal, round_trips_a_local_transform_under_a_transformed_parent)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 10.0f, 5.0f, -2.0f }, { 0.0f, 40.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);
    PushEntity(sceneUnderTest, { 1.0f, 2.0f, 3.0f }, { 15.0f, 0.0f, -30.0f }, { 1.0f, 1.0f, 1.0f }, 0);

    const Mat4 parentWorld = quark::ComposeWorld(sceneUnderTest, 0);
    const Mat4 childWorld = quark::ComposeWorld(sceneUnderTest, 1);

    CTransformComponent result;
    quark::DecomposeLocal(parentWorld, childWorld, result);

    ExpectVecNear(result.m_Position, sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Position, 1e-2);
    ExpectVecNear(result.m_Rotation, sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Rotation, 1.0);
    ExpectVecNear(result.m_Scale, sceneUnderTest.m_vEntities[1].GetTransformComponent()->m_Scale, 1e-2);
}

TEST(DecomposeLocal, parent_identity_equals_decompose_world)
{
    CTransformComponent source;
    source.m_Position = { -1.0f, 4.0f, 0.5f };
    source.m_Rotation = { 0.0f, 90.0f, 0.0f };
    source.m_Scale = { 3.0f, 3.0f, 3.0f };

    const Mat4 world = quark::ComposeLocal(source);

    CTransformComponent viaDecomposeWorld;
    CTransformComponent viaDecomposeLocal;
    quark::DecomposeWorld(world, viaDecomposeWorld);
    quark::DecomposeLocal(Mat4::identity(), world, viaDecomposeLocal);

    ExpectVecNear(viaDecomposeLocal.m_Position, viaDecomposeWorld.m_Position, 1e-4);
    ExpectVecNear(viaDecomposeLocal.m_Rotation, viaDecomposeWorld.m_Rotation, 1e-4);
    ExpectVecNear(viaDecomposeLocal.m_Scale, viaDecomposeWorld.m_Scale, 1e-4);
}

TEST(ParentWorld, root_entity_has_an_identity_parent_world)
{
    CScene sceneUnderTest;
    const int index = PushEntity(sceneUnderTest, { 4.0f, 5.0f, 6.0f }, { 0.0f, 45.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);

    ExpectMatNear(quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[index]), Mat4::identity(), 1e-6);
}

TEST(ParentWorld, returns_the_whole_parent_chain_world_transform)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 1.0f, 0.0f, 0.0f }, { 0.0f, 30.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);
    PushEntity(sceneUnderTest, { 0.0f, 2.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 0);
    const int leaf = PushEntity(sceneUnderTest, { 0.0f, 0.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 1);

    ExpectMatNear(quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[leaf]),
        quark::ComposeWorld(sceneUnderTest, 1), 1e-5);
}

TEST(ParentWorld, times_local_equals_compose_world)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 10.0f, -4.0f, 2.0f }, { 0.0f, 65.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);
    const int child = PushEntity(sceneUnderTest, { 1.0f, 2.0f, 3.0f }, { 15.0f, 0.0f, -30.0f }, { 1.0f, 1.0f, 1.0f }, 0);

    const Mat4 handRolled = quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[child])
        * quark::ComposeLocal(sceneUnderTest.m_vEntities[child]);

    ExpectMatNear(handRolled, quark::ComposeWorld(sceneUnderTest, child), 1e-4);
}

TEST(ParentWorld, self_parent_returns_identity)
{
    CScene sceneUnderTest;
    const int index = PushEntity(sceneUnderTest, { 2.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 3.0f, 3.0f, 3.0f }, 0);

    ExpectMatNear(quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[index]), Mat4::identity(), 1e-6);
}

TEST(ParentWorld, out_of_range_parent_id_returns_identity)
{
    CScene sceneUnderTest;
    const int index = PushEntity(sceneUnderTest, { 2.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 42);

    ExpectMatNear(quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[index]), Mat4::identity(), 1e-6);
}

TEST(ParentWorld, foreign_entity_returns_identity)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, -1);

    CEntity foreign;
    foreign.m_ParentId = 0;

    ExpectMatNear(quark::ParentWorld(sceneUnderTest, foreign), Mat4::identity(), 1e-6);
}

TEST(DecomposeLocal, gizmo_edit_round_trip_keeps_the_world_transform_stable)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 6.0f, -2.0f, 4.0f }, { 0.0f, 50.0f, 0.0f }, { 1.5f, 1.5f, 1.5f }, -1);
    const int child = PushEntity(sceneUnderTest, { 1.0f, 2.0f, 3.0f }, { 15.0f, 0.0f, -30.0f }, { 1.0f, 1.0f, 1.0f }, 0);

    const Mat4 before = quark::ComposeWorld(sceneUnderTest, child);

    CTransformComponent edited;
    quark::DecomposeLocal(quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[child]), before, edited);

    const Mat4 after = quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[child]) * quark::ComposeLocal(edited);

    ExpectMatNear(after, before, 1e-2);
}

TEST(DecomposeLocal, child_local_is_unchanged_by_a_parent_world_transform)
{
    CScene sceneUnderTest;
    PushEntity(sceneUnderTest, { 10.0f, 5.0f, -2.0f }, { 0.0f, 40.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, -1);
    PushEntity(sceneUnderTest, { 1.0f, 2.0f, 3.0f }, { 15.0f, 0.0f, -30.0f }, { 1.0f, 1.0f, 1.0f }, 0);

    CTransformComponent* pChild = sceneUnderTest.m_vEntities[1].GetTransformComponent();
    const CTransformComponent original = *pChild;

    quark::DecomposeLocal(quark::ParentWorld(sceneUnderTest, sceneUnderTest.m_vEntities[1]),
        quark::ComposeWorld(sceneUnderTest, 1), *pChild);

    ExpectVecNear(pChild->m_Position, original.m_Position, 1e-2);
    ExpectVecNear(pChild->m_Rotation, original.m_Rotation, 1.0);
    ExpectVecNear(pChild->m_Scale, original.m_Scale, 1e-2);
}

TEST(DecomposeLocal, snaps_near_zero_and_near_one)
{
    CTransformComponent source;
    source.m_Position = { 0.0f, 0.0f, 0.0f };
    source.m_Rotation = { 0.0f, 0.0f, 0.0f };
    source.m_Scale = { 1.0f, 1.0f, 1.0f };

    CTransformComponent result;
    result.m_Scale = { 0.0f, 5.0f, 1.0f };
    quark::DecomposeLocal(Mat4::identity(), quark::ComposeLocal(source), result);

    CHECK_NEAR(result.m_Position.x, 0.0f, 1e-6);
    CHECK_NEAR(result.m_Scale.y, 1.0f, 1e-3);
}
