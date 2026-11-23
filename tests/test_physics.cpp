#include "test_harness.h"

#include "QuarkEngine/SDK.h"
#include "engine/physics_world.h"
#include "engine/scene_document.h"
#include "scene.h"
#include "scripting/script_engine.h"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace
{
    CEntity& AddPhysicsEntity(CScene& scene, int id, const std::string& name, const Vec3& pos)
    {
        CEntity entity(id);
        entity.m_Name = name;
        entity.GetTransformComponent()->m_Position = pos;
        scene.m_vEntities.push_back(std::move(entity));
        return scene.m_vEntities.back();
    }

    CEntity& AddBoxEntity(CScene& scene, const std::string& name,
                          const Vec3& pos, const Vec3& size,
                          bool isTrigger = false)
    {
        CEntity entity(static_cast<int>(scene.m_vEntities.size()));
        entity.m_Name = name;
        entity.GetTransformComponent()->m_Position = pos;

        auto pCol = std::make_shared<CCollisionComponent>();
        pCol->m_ColliderType = COLLIDER_BOX;
        pCol->m_Size = size;
        pCol->m_IsTrigger = isTrigger;
        entity.GetComponents()->AddComponent(pCol);

        scene.m_vEntities.push_back(std::move(entity));
        return scene.m_vEntities.back();
    }

    CEntity& AddSphereEntity(CScene& scene, const std::string& name,
                             const Vec3& pos, float radius)
    {
        CEntity entity(static_cast<int>(scene.m_vEntities.size()));
        entity.m_Name = name;
        entity.GetTransformComponent()->m_Position = pos;

        auto pCol = std::make_shared<CCollisionComponent>();
        pCol->m_ColliderType = COLLIDER_SPHERE;
        pCol->m_Radius = radius;
        entity.GetComponents()->AddComponent(pCol);

        scene.m_vEntities.push_back(std::move(entity));
        return scene.m_vEntities.back();
    }

    CEntity& AddCapsuleEntity(CScene& scene, const std::string& name,
                              const Vec3& pos, float radius, float height)
    {
        CEntity entity(static_cast<int>(scene.m_vEntities.size()));
        entity.m_Name = name;
        entity.GetTransformComponent()->m_Position = pos;

        auto pCol = std::make_shared<CCollisionComponent>();
        pCol->m_ColliderType = COLLIDER_CAPSULE;
        pCol->m_Radius = radius;
        pCol->m_Height = height;
        entity.GetComponents()->AddComponent(pCol);

        scene.m_vEntities.push_back(std::move(entity));
        return scene.m_vEntities.back();
    }
} // anonymous

TEST(Physics, GravityAndBoxFloorCollision)
{
    CScene scene;
    AddBoxEntity(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 20.0f, 1.0f, 20.0f });
    CEntity& box = AddBoxEntity(scene, "FallingBox", { 0.0f, 3.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_Restitution = 0.0f;
    box.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    bool sawEnter = false;
    bool sawStay = false;

    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int step = 0; step < 120; ++step)
    {
        world.Step(scene, 0.02f, vEvents);
        for (const auto& ev : vEvents)
        {
            if (ev.m_Type == quark::physics::EPhysicsEventType::COLLISION_ENTER)
            {
                sawEnter = true;
            }
            if (ev.m_Type == quark::physics::EPhysicsEventType::COLLISION_STAY)
            {
                sawStay = true;
            }
        }
    }

    CHECK(sawEnter);
    CHECK(sawStay);
    CHECK(scene.m_vEntities[1].GetRigidbodyComponent()->m_IsGrounded);
    CHECK_NEAR(scene.m_vEntities[1].GetTransformComponent()->m_Position.y, 0.5f, 0.05f);
}

TEST(Physics, SphereBounceAndCapsuleLanding)
{
    CScene scene;
    AddBoxEntity(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 20.0f, 1.0f, 20.0f });

    CEntity& sphere = AddSphereEntity(scene, "Ball", { -2.0f, 2.0f, 0.0f }, 0.5f);
    auto pSphereBody = std::make_shared<CRigidbodyComponent>();
    pSphereBody->m_Restitution = 0.8f;
    pSphereBody->m_LinearDrag = 0.0f;
    sphere.GetComponents()->AddComponent(pSphereBody);

    CEntity& capsule = AddCapsuleEntity(scene, "Player", { 2.0f, 2.5f, 0.0f }, 0.5f, 2.0f);
    auto pCapBody = std::make_shared<CRigidbodyComponent>();
    pCapBody->m_Restitution = 0.0f;
    capsule.GetComponents()->AddComponent(pCapBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    bool ballBouncedUp = false;
    for (int step = 0; step < 120; ++step)
    {
        world.Step(scene, 0.02f, vEvents);
        if (scene.m_vEntities[1].GetRigidbodyComponent()->m_Velocity.y > 1.0f)
        {
            ballBouncedUp = true;
        }
    }

    CHECK(ballBouncedUp);
    CHECK(scene.m_vEntities[2].GetRigidbodyComponent()->m_IsGrounded);
    CHECK_NEAR(scene.m_vEntities[2].GetTransformComponent()->m_Position.y, 1.0f, 0.05f);
}

TEST(Physics, TriggerEnterStayExitLifecycle)
{
    CScene scene;
    AddBoxEntity(scene, "Zone", { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, true);
    CEntity& mover = AddSphereEntity(scene, "Mover", { -2.5f, 0.0f, 0.0f }, 0.5f);
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_UseGravity = false;
    pBody->m_LinearDrag = 0.0f;
    pBody->m_Velocity = { 5.0f, 0.0f, 0.0f };
    mover.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    int enterCount = 0;
    int stayCount = 0;
    int exitCount = 0;

    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int step = 0; step < 80; ++step)
    {
        world.Step(scene, 0.02f, vEvents);
        for (const auto& ev : vEvents)
        {
            if (ev.m_Type == quark::physics::EPhysicsEventType::TRIGGER_ENTER)
            {
                ++enterCount;
            }
            else if (ev.m_Type == quark::physics::EPhysicsEventType::TRIGGER_STAY)
            {
                ++stayCount;
            }
            else if (ev.m_Type == quark::physics::EPhysicsEventType::TRIGGER_EXIT)
            {
                ++exitCount;
            }
        }
    }

    CHECK(enterCount == 1);
    CHECK(stayCount > 0);
    CHECK(exitCount == 1);
    CHECK(scene.m_vEntities[1].GetTransformComponent()->m_Position.x > 2.0f);
}

TEST(Physics, RaycastHitsBoxSphereAndCapsule)
{
    CScene scene;
    AddBoxEntity(scene, "Box", { 0.0f, 0.0f, -5.0f }, { 2.0f, 2.0f, 2.0f });
    AddSphereEntity(scene, "Sphere", { 5.0f, 0.0f, -5.0f }, 1.0f);
    AddCapsuleEntity(scene, "Capsule", { -5.0f, 0.0f, -5.0f }, 0.5f, 2.0f);
    AddBoxEntity(scene, "Trigger", { 0.0f, 0.0f, -2.0f }, { 2.0f, 2.0f, 2.0f }, true);

    quark::physics::CPhysicsWorld world;
    quark::physics::SPhysicsRaycastHit hit;

    CHECK(world.Raycast(scene, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 50.0f, hit, false));
    CHECK(hit.m_EntityId == 0);
    CHECK_NEAR(hit.m_Distance, 4.0f, 0.01f);
    CHECK_NEAR(hit.m_Normal.z, 1.0f, 0.01f);

    CHECK(world.Raycast(scene, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 50.0f, hit, true));
    CHECK(hit.m_EntityId == 3);
    CHECK_NEAR(hit.m_Distance, 1.0f, 0.01f);

    CHECK(world.Raycast(scene, { 5.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 50.0f, hit, false));
    CHECK(hit.m_EntityId == 1);
    CHECK_NEAR(hit.m_Distance, 4.0f, 0.01f);

    CHECK(world.Raycast(scene, { -5.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 50.0f, hit, false));
    CHECK(hit.m_EntityId == 2);
    CHECK_NEAR(hit.m_Distance, 4.5f, 0.01f);

    CHECK(!world.Raycast(scene, { -5.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 50.0f, hit, false, 2));
}

TEST(Physics, RigidbodySerializationRoundTrip)
{
    CRigidbodyComponent original;
    original.m_Mass = 4.5f;
    original.m_LinearDrag = 0.25f;
    original.m_AngularDrag = 12.5f;
    original.m_Friction = 0.7f;
    original.m_Restitution = 0.4f;
    original.m_UseGravity = false;
    original.m_IsKinematic = true;
    original.m_Interpolate = ERigidbodyInterpolate::INTERPOLATE;
    original.m_CollisionDetection = ECollisionDetectionMode::CONTINUOUS_DYNAMIC;
    original.m_FreezePositionX = true;
    original.m_FreezePositionY = false;
    original.m_FreezePositionZ = true;
    original.m_FreezeRotationX = true;
    original.m_FreezeRotationY = false;
    original.m_FreezeRotationZ = true;

    nlohmann::json data;
    original.Serialize(data);

    CRigidbodyComponent restored;
    restored.Deserialize(data);

    CHECK_NEAR(restored.m_Mass, 4.5f, 0.001f);
    CHECK_NEAR(restored.m_LinearDrag, 0.25f, 0.001f);
    CHECK_NEAR(restored.m_AngularDrag, 12.5f, 0.001f);
    CHECK_NEAR(restored.m_Friction, 0.7f, 0.001f);
    CHECK_NEAR(restored.m_Restitution, 0.4f, 0.001f);
    CHECK(!restored.m_UseGravity);
    CHECK(restored.m_IsKinematic);
    CHECK(restored.m_Interpolate == ERigidbodyInterpolate::INTERPOLATE);
    CHECK(restored.m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS_DYNAMIC);
    CHECK(restored.m_FreezePositionX);
    CHECK(!restored.m_FreezePositionY);
    CHECK(restored.m_FreezePositionZ);
    CHECK(restored.m_FreezeRotationX);
    CHECK(!restored.m_FreezeRotationY);
    CHECK(restored.m_FreezeRotationZ);
}

TEST(Physics, ThinPlatformDoesNotSinkLikeQuicksand)
{
    CScene scene;
    CEntity& platform = AddBoxEntity(scene, "ThinPlatform", { 0.0f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    platform.GetTransformComponent()->m_Scale = { 3.855f, 0.001f, 3.844f };

    CEntity& cube = AddBoxEntity(scene, "Cube", { 0.0f, 2.937f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_Restitution = 0.2f;
    cube.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int step = 0; step < 300; ++step)
    {
        world.Step(scene, 0.02f, vEvents);
    }

    CHECK(scene.m_vEntities[1].GetRigidbodyComponent()->m_IsGrounded);
    CHECK_NEAR(scene.m_vEntities[1].GetTransformComponent()->m_Position.y, 0.0005f, 0.01f);
}

TEST(Physics, ContinuousCollisionPreventsTunnelingThroughThinWall)
{
    CScene scene;
    AddBoxEntity(scene, "ThinWall", { 2.0f, 0.0f, 0.0f }, { 0.02f, 10.0f, 10.0f });

    CEntity& bullet = AddBoxEntity(scene, "Bullet", { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.2f, 0.2f });
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_UseGravity = false;
    pBody->m_LinearDrag = 0.0f;
    pBody->m_Restitution = 0.0f;
    pBody->m_CollisionDetection = ECollisionDetectionMode::CONTINUOUS;
    pBody->m_Velocity = { 150.0f, 0.0f, 0.0f };
    bullet.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    world.Step(scene, 0.02f, vEvents);

    CHECK(scene.m_vEntities[1].GetTransformComponent()->m_Position.x < 2.0f);
    CHECK(!vEvents.empty());
}

TEST(Physics, AngularDynamicsDragAndFreezeRotation)
{
    CScene scene;
    CEntity& spinner = AddBoxEntity(scene, "Spinner", { 0.0f, 5.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_UseGravity = false;
    pBody->m_AngularDrag = 2.0f;
    pBody->m_FreezeRotationX = true;
    pBody->m_FreezeRotationY = false;
    pBody->m_FreezeRotationZ = true;
    pBody->AddTorque({ 10.0f, 10.0f, 10.0f });
    spinner.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    world.Step(scene, 0.02f, vEvents);

    const CRigidbodyComponent* pResultBody = scene.m_vEntities[0].GetRigidbodyComponent();
    const CTransformComponent* pResultTransform = scene.m_vEntities[0].GetTransformComponent();
    CHECK_NEAR(pResultBody->m_AngularVelocity.x, 0.0f, 0.0001f);
    CHECK(pResultBody->m_AngularVelocity.y > 0.1f);
    CHECK_NEAR(pResultBody->m_AngularVelocity.z, 0.0f, 0.0001f);
    CHECK(pResultTransform->m_Rotation.y > 0.0f);

    const float initialYawSpeed = pResultBody->m_AngularVelocity.y;
    for (int i = 0; i < 20; ++i)
    {
        world.Step(scene, 0.02f, vEvents);
    }
    CHECK(scene.m_vEntities[0].GetRigidbodyComponent()->m_AngularVelocity.y < initialYawSpeed);
}

TEST(Physics, InterpolationAndExtrapolationSmoothMotion)
{
    CScene scene;
    CEntity& interpEntity = AddBoxEntity(scene, "Interp", { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    auto pInterpBody = std::make_shared<CRigidbodyComponent>();
    pInterpBody->m_UseGravity = false;
    pInterpBody->m_LinearDrag = 0.0f;
    pInterpBody->m_Interpolate = ERigidbodyInterpolate::INTERPOLATE;
    pInterpBody->m_Velocity = { 10.0f, 0.0f, 0.0f };
    interpEntity.GetComponents()->AddComponent(pInterpBody);

    CEntity& extrapEntity = AddBoxEntity(scene, "Extrap", { 0.0f, 5.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    auto pExtrapBody = std::make_shared<CRigidbodyComponent>();
    pExtrapBody->m_UseGravity = false;
    pExtrapBody->m_LinearDrag = 0.0f;
    pExtrapBody->m_Interpolate = ERigidbodyInterpolate::EXTRAPOLATE;
    pExtrapBody->m_Velocity = { 10.0f, 0.0f, 0.0f };
    extrapEntity.GetComponents()->AddComponent(pExtrapBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    world.Step(scene, 0.02f, vEvents);

    world.ApplyInterpolation(scene, 0.5f, 0.01f);
    CHECK_NEAR(scene.m_vEntities[0].GetTransformComponent()->m_Position.x, 0.1f, 0.001f);
    CHECK_NEAR(scene.m_vEntities[1].GetTransformComponent()->m_Position.x, 0.3f, 0.001f);
}

TEST(Physics, TriangleMeshColliderRampAndRaycast)
{
    CScene scene;
    auto& ramp = AddPhysicsEntity(scene, 0, "Ramp", { 0.0f, 0.0f, 0.0f });
    CMeshComponent* pRampMesh = ramp.GetMeshComponent();
    pRampMesh->m_IsEditableMesh = true;
    pRampMesh->m_EditableMesh.m_vVertices = {
        { { -2.0f, 2.0f, -2.0f } },
        { {  2.0f, 0.0f, -2.0f } },
        { {  2.0f, 0.0f,  2.0f } },
        { { -2.0f, 2.0f,  2.0f } }
    };
    pRampMesh->m_EditableMesh.m_vTriangles = { { 0, 2, 1 }, { 0, 3, 2 } };

    auto pRampCol = std::make_shared<CCollisionComponent>();
    pRampCol->m_ColliderType = COLLIDER_MESH;
    pRampCol->m_Convex = false;
    ramp.GetComponents()->AddComponent(pRampCol);

    auto& ball = AddPhysicsEntity(scene, 1, "Ball", { 0.0f, 2.0f, 0.0f });
    auto pBallCol = std::make_shared<CCollisionComponent>();
    pBallCol->m_ColliderType = COLLIDER_SPHERE;
    pBallCol->m_Radius = 0.5f;
    ball.GetComponents()->AddComponent(pBallCol);
    auto pBallBody = std::make_shared<CRigidbodyComponent>();
    pBallBody->m_Friction = 0.05f;
    pBallBody->m_Restitution = 0.0f;
    ball.GetComponents()->AddComponent(pBallBody);

    quark::physics::CPhysicsWorld world;
    quark::physics::SPhysicsRaycastHit hit;
    CHECK(world.Raycast(scene, { 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, hit, false, 1));
    CHECK(hit.m_EntityId == 0);
    CHECK_NEAR(hit.m_Point.y, 1.0f, 0.05f);
    CHECK(hit.m_Normal.x > 0.3f);
    CHECK(hit.m_Normal.y > 0.7f);

    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int i = 0; i < 60; ++i)
    {
        world.Step(scene, 1.0f / 60.0f, vEvents);
    }

    const Vec3 ballPos = scene.m_vEntities[1].GetTransformComponent()->m_Position;
    CHECK(ballPos.x > 0.1f);
    CHECK(ballPos.y > 0.2f);
}

TEST(Physics, ConvexMeshDynamicVsConcaveMeshStatic)
{
    CScene scene;
    auto& floor = AddPhysicsEntity(scene, 0, "Floor", { 0.0f, -0.5f, 0.0f });
    auto pFloorCol = std::make_shared<CCollisionComponent>();
    pFloorCol->m_ColliderType = COLLIDER_BOX;
    pFloorCol->m_Size = { 10.0f, 1.0f, 10.0f };
    floor.GetComponents()->AddComponent(pFloorCol);

    auto fillEditableCubeMesh = [](CMeshComponent* pMesh)
    {
        pMesh->m_IsEditableMesh = true;
        pMesh->m_EditableMesh.m_vVertices = {
            { { -0.5f, -0.5f, -0.5f } }, { {  0.5f, -0.5f, -0.5f } },
            { {  0.5f,  0.5f, -0.5f } }, { { -0.5f,  0.5f, -0.5f } },
            { { -0.5f, -0.5f,  0.5f } }, { {  0.5f, -0.5f,  0.5f } },
            { {  0.5f,  0.5f,  0.5f } }, { { -0.5f,  0.5f,  0.5f } }
        };
        pMesh->m_EditableMesh.m_vTriangles = {
            { 0, 1, 2 }, { 0, 2, 3 },
            { 4, 6, 5 }, { 4, 7, 6 },
            { 0, 4, 5 }, { 0, 5, 1 },
            { 3, 2, 6 }, { 3, 6, 7 },
            { 0, 3, 7 }, { 0, 7, 4 },
            { 1, 5, 6 }, { 1, 6, 2 }
        };
    };

    auto& concaveCube = AddPhysicsEntity(scene, 1, "ConcaveMesh", { -2.0f, 3.0f, 0.0f });
    fillEditableCubeMesh(concaveCube.GetMeshComponent());
    auto pConcaveCol = std::make_shared<CCollisionComponent>();
    pConcaveCol->m_ColliderType = COLLIDER_MESH;
    pConcaveCol->m_Convex = false;
    concaveCube.GetComponents()->AddComponent(pConcaveCol);
    concaveCube.GetComponents()->AddComponent(std::make_shared<CRigidbodyComponent>());

    auto& convexCube = AddPhysicsEntity(scene, 2, "ConvexMesh", { 2.0f, 3.0f, 0.0f });
    fillEditableCubeMesh(convexCube.GetMeshComponent());
    auto pConvexCol = std::make_shared<CCollisionComponent>();
    pConvexCol->m_ColliderType = COLLIDER_MESH;
    pConvexCol->m_Convex = true;
    convexCube.GetComponents()->AddComponent(pConvexCol);
    auto pConvexBody = std::make_shared<CRigidbodyComponent>();
    pConvexBody->m_Restitution = 0.0f;
    convexCube.GetComponents()->AddComponent(pConvexBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int i = 0; i < 90; ++i)
    {
        world.Step(scene, 1.0f / 60.0f, vEvents);
    }

    CHECK_NEAR(scene.m_vEntities[1].GetTransformComponent()->m_Position.y, 3.0f, 0.001f);
    const float convexY = scene.m_vEntities[2].GetTransformComponent()->m_Position.y;
    CHECK(convexY >= 0.45f && convexY <= 0.65f);
}

TEST(Physics, CollisionDetectionModesPreventTunneling)
{
    auto runSpeculativeTest = [](ECollisionDetectionMode mode) -> float
    {
        CScene scene;
        auto& wall = AddPhysicsEntity(scene, 0, "Wall", { 1.0f, 0.0f, 0.0f });
        auto pWallCol = std::make_shared<CCollisionComponent>();
        pWallCol->m_ColliderType = COLLIDER_BOX;
        pWallCol->m_Size = { 0.1f, 4.0f, 4.0f };
        wall.GetComponents()->AddComponent(pWallCol);

        auto& bullet = AddPhysicsEntity(scene, 1, "Bullet", { -1.0f, 0.0f, 0.0f });
        auto pBulletCol = std::make_shared<CCollisionComponent>();
        pBulletCol->m_ColliderType = COLLIDER_SPHERE;
        pBulletCol->m_Radius = 0.25f;
        bullet.GetComponents()->AddComponent(pBulletCol);
        auto pBody = std::make_shared<CRigidbodyComponent>();
        pBody->m_UseGravity = false;
        pBody->m_Velocity = { 200.0f, 0.0f, 0.0f };
        pBody->m_CollisionDetection = mode;
        pBody->m_Restitution = 0.0f;
        bullet.GetComponents()->AddComponent(pBody);

        quark::physics::CPhysicsWorld world;
        std::vector<quark::physics::SPhysicsEvent> vEvents;
        for (int i = 0; i < 5; ++i)
        {
            world.Step(scene, 1.0f / 60.0f, vEvents);
        }
        return scene.m_vEntities[1].GetTransformComponent()->m_Position.x;
    };

    CHECK(runSpeculativeTest(ECollisionDetectionMode::CONTINUOUS) < 1.0f);
    CHECK(runSpeculativeTest(ECollisionDetectionMode::CONTINUOUS_DYNAMIC) < 1.0f);
    CHECK(runSpeculativeTest(ECollisionDetectionMode::CONTINUOUS_SPECULATIVE) < 1.0f);
}

TEST(Physics, InertiaTensorRotationForCapsuleAndMesh)
{
    CScene scene;
    auto& capX = AddPhysicsEntity(scene, 0, "CapX", { 0.0f, 0.0f, 0.0f });
    auto pCapCol = std::make_shared<CCollisionComponent>();
    pCapCol->m_ColliderType = COLLIDER_CAPSULE;
    pCapCol->m_Direction = ECapsuleDirection::X_AXIS;
    capX.GetComponents()->AddComponent(pCapCol);

    const auto capInfo = quark::physics::CPhysicsWorld::ComputeInertiaTensor(capX, 2.0f);
    CHECK_NEAR(capInfo.m_RotationDeg.z, 90.0f, 0.01f);

    auto& skewedMesh = AddPhysicsEntity(scene, 1, "SkewedMesh", { 0.0f, 0.0f, 0.0f });
    CMeshComponent* pMesh = skewedMesh.GetMeshComponent();
    pMesh->m_IsEditableMesh = true;
    pMesh->m_EditableMesh.m_vVertices = {
        { { -2.0f, -1.0f, 0.0f } },
        { {  2.0f,  1.0f, 0.0f } },
        { {  0.0f,  0.0f, 0.5f } },
        { {  0.0f,  0.0f, -0.5f } }
    };
    auto pMeshCol = std::make_shared<CCollisionComponent>();
    pMeshCol->m_ColliderType = COLLIDER_MESH;
    skewedMesh.GetComponents()->AddComponent(pMeshCol);

    const auto meshInfo = quark::physics::CPhysicsWorld::ComputeInertiaTensor(skewedMesh, 3.0f);
    CHECK(std::fabs(meshInfo.m_RotationDeg.z) > 10.0f);
}

TEST(Physics, KickedCubeStopsSlidingAndRollingOnFloor)
{
    CScene scene;
    AddBoxEntity(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 50.0f, 1.0f, 50.0f });
    CEntity& cube = AddBoxEntity(scene, "Cube", { 0.0f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_Velocity = { 5.0f, 0.0f, 0.0f };
    pBody->m_AngularVelocity = { 0.0f, 0.0f, -5.0f };
    pBody->m_Friction = 0.5f;
    pBody->m_Restitution = 0.0f;
    cube.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int i = 0; i < 120; ++i)
    {
        world.Step(scene, 1.0f / 60.0f, vEvents);
    }

    const CRigidbodyComponent* pFinalBody = scene.m_vEntities[1].GetRigidbodyComponent();
    const CTransformComponent* pFinalTransform = scene.m_vEntities[1].GetTransformComponent();
    CHECK(pFinalTransform->m_Position.x > 0.5f);
    CHECK(pFinalTransform->m_Position.x < 4.5f);
    CHECK_NEAR(pFinalBody->m_Velocity.x, 0.0f, 0.01f);
    CHECK_NEAR(pFinalBody->m_AngularVelocity.length(), 0.0f, 0.01f);
}

TEST(Physics, TiltedCubeFallsOffEdgeOntoFlatFace)
{
    CScene scene;
    AddBoxEntity(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 50.0f, 1.0f, 50.0f });
    CEntity& cube = AddBoxEntity(scene, "Cube", { 0.0f, 0.72f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    cube.GetTransformComponent()->m_Rotation = { 0.0f, 0.0f, 38.0f };
    auto pBody = std::make_shared<CRigidbodyComponent>();
    pBody->m_Friction = 0.5f;
    pBody->m_Restitution = 0.0f;
    cube.GetComponents()->AddComponent(pBody);

    quark::physics::CPhysicsWorld world;
    std::vector<quark::physics::SPhysicsEvent> vEvents;
    for (int i = 0; i < 120; ++i)
    {
        world.Step(scene, 1.0f / 60.0f, vEvents);
    }

    const float finalRz = scene.m_vEntities[1].GetTransformComponent()->m_Rotation.z;
    const float distToFlat = std::fabs(finalRz - std::round(finalRz / 90.0f) * 90.0f);
    CHECK(distToFlat < 1.0f);

    CScene scene3D;
    AddBoxEntity(scene3D, "Floor", { 0.0f, -0.5f, 0.0f }, { 100.0f, 1.0f, 100.0f });
    CEntity& cube3D = AddBoxEntity(scene3D, "Cube3D", { 0.0f, 0.86f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    cube3D.GetTransformComponent()->m_Rotation = { 35.264f, 47.0f, 45.0f };
    auto pBody3D = std::make_shared<CRigidbodyComponent>();
    pBody3D->m_Velocity = { 12.0f, -2.0f, -9.0f };
    pBody3D->m_AngularVelocity = { 14.0f, 19.0f, -16.0f };
    pBody3D->m_Friction = 0.5f;
    pBody3D->m_Restitution = 0.2f;
    cube3D.GetComponents()->AddComponent(pBody3D);

    quark::physics::CPhysicsWorld world3D;
    for (int i = 0; i < 180; ++i)
    {
        world3D.Step(scene3D, 1.0f / 60.0f, vEvents);
    }

    const Vec3 rot3D = scene3D.m_vEntities[1].GetTransformComponent()->m_Rotation;
    const Mat4 finalRotMat =
        Mat4::rotationY(rot3D.y * DEG2RAD) *
        Mat4::rotationX(rot3D.x * DEG2RAD) *
        Mat4::rotationZ(rot3D.z * DEG2RAD);
    const float maxUpAlign = std::max({
        std::fabs(finalRotMat.m[1]),
        std::fabs(finalRotMat.m[5]),
        std::fabs(finalRotMat.m[9])
    });
    CHECK(maxUpAlign > 0.999f);
}

namespace
{
    class CTestTriggerLoggerScript : public IScript
    {
    public:
        std::string m_TriggerMessage = "Trigger activated!";

        void OnTriggerEnter(ScriptEntity* pOther) override
        {
            const std::string otherName = pOther ? pOther->GetName() : "Unknown";
            Debug::Log("TRIGGER_ENTER:" + m_TriggerMessage + ":" + otherName);
        }
    };
} // anonymous

REGISTER_SCRIPT(CTestTriggerLoggerScript)

TEST(Physics, ScriptApiTriggerLoggingRaycastOverlapAndSpawn)
{
    auto& engine = quark::script::CScriptEngine::Get();
    engine.ClearLogs();

    CScene scene;
    AddBoxEntity(scene, "Floor", { 0.0f, -0.5f, 0.0f }, { 20.0f, 1.0f, 20.0f }, false);

    CEntity& triggerCube = AddBoxEntity(scene, "TriggerCube", { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, true);
    auto pScriptComp = std::make_shared<quark::script::CScriptComponent>();
    pScriptComp->m_ScriptName = "CTestTriggerLoggerScript";
    quark::script::SScriptField msgField;
    msgField.m_Name = "m_TriggerMessage";
    msgField.m_Type = quark::script::EScriptFieldType::STRING;
    msgField.m_StringValue = "Custom trigger log";
    pScriptComp->m_vFields.push_back(msgField);
    triggerCube.GetComponents()->AddComponent(pScriptComp);

    CEntity& player = AddCapsuleEntity(scene, "Player", { -2.0f, 1.0f, 0.0f }, 0.5f, 2.0f);
    auto pPlayerBody = std::make_shared<CRigidbodyComponent>();
    pPlayerBody->m_UseGravity = false;
    pPlayerBody->m_LinearDrag = 0.0f;
    pPlayerBody->m_Velocity = { 5.0f, 0.0f, 0.0f };
    player.GetComponents()->AddComponent(pPlayerBody);

    CEntity& lightEnt = AddPhysicsEntity(scene, 3, "SceneLight", { 0.0f, 4.0f, 0.0f });
    lightEnt.GetComponents()->AddComponent(std::make_shared<CLightComponent>());

    engine.StartPlay(scene, "");
    Physics::SetVelocity(Scene::FindEntityByName("Player"), { 5.0f, 0.0f, 0.0f });
    for (int i = 0; i < 30; ++i)
    {
        engine.UpdatePlay(scene, 0.02f);
    }

    bool sawPlayerTriggerLog = false;
    bool sawFloorTriggerLog = false;
    for (const auto& entry : engine.GetLogs())
    {
        if (entry.m_Message == "TRIGGER_ENTER:Custom trigger log:Player")
        {
            sawPlayerTriggerLog = true;
        }
        if (entry.m_Message.find(":Floor") != std::string::npos)
        {
            sawFloorTriggerLog = true;
        }
    }
    CHECK(sawPlayerTriggerLog);
    CHECK(!sawFloorTriggerLog);

    ScriptEntity* pSpawned = Scene::CreatePrimitive(
        EScriptPrimitiveType::CUBE, "SpawnedCube", { 0.0f, 0.0f, -5.0f }, { 1.0f, 1.0f, 1.0f });
    CHECK(pSpawned != nullptr);
    MaterialAPI::SetColor(pSpawned, { 255, 64, 32, 255 });
    CHECK(MaterialAPI::GetColor(pSpawned).r == 255);
    CHECK(MaterialAPI::GetColor(pSpawned).g == 64);

    ScriptEntity* pLightWrapper = Scene::FindEntityByName("SceneLight");
    CHECK(pLightWrapper != nullptr);
    LightAPI::SetIntensity(pLightWrapper, 3.25f);
    LightAPI::SetRange(pLightWrapper, 24.0f);
    CHECK_NEAR(LightAPI::GetIntensity(pLightWrapper), 3.25f, 0.001f);
    CHECK_NEAR(LightAPI::GetRange(pLightWrapper), 24.0f, 0.001f);

    const auto vHits = Physics::RaycastAll({ 0.0f, 0.0f, 2.0f }, { 0.0f, 0.0f, -1.0f }, 20.0f, true);
    CHECK(vHits.size() >= 2);
    const auto vOverlaps = Physics::OverlapSphere({ 0.0f, 0.0f, -5.0f }, 1.0f, false);
    CHECK(!vOverlaps.empty());

    engine.StopPlay(scene);
}

namespace
{
    class CPersistentTrackerScript final : public quark::script::IScript
    {
    public:
        static inline int ms_StartCount = 0;
        static inline int ms_DestroyCount = 0;
        static inline bool ms_RequestLoadOnUpdate = false;
        static inline std::string ms_TargetScene;

        void OnStart() override
        {
            ++ms_StartCount;
            quark::script::SceneManager::DontDestroyOnLoad(m_Entity);
        }

        void OnUpdate(float) override
        {
            if (ms_RequestLoadOnUpdate)
            {
                ms_RequestLoadOnUpdate = false;
                quark::script::SceneManager::LoadScene(ms_TargetScene);
            }
        }

        void OnDestroy() override
        {
            ++ms_DestroyCount;
        }
    };

} // anonymous

REGISTER_SCRIPT(CPersistentTrackerScript)

TEST(SceneManager, DeferredLoadDontDestroyAndPersistentData)
{
    using namespace quark::script;
    namespace fs = std::filesystem;

    CPersistentTrackerScript::ms_StartCount = 0;
    CPersistentTrackerScript::ms_DestroyCount = 0;
    CPersistentTrackerScript::ms_RequestLoadOnUpdate = false;
    CPersistentTrackerScript::ms_TargetScene.clear();

    const fs::path tempRoot = fs::temp_directory_path() / "quark_scene_manager_test";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot / "resources", ec);

    CScene level1;
    CEntity persistentParent;
    persistentParent.m_Id = 0;
    persistentParent.m_Name = "PersistentManager";
    auto pScriptComp = std::make_shared<CScriptComponent>();
    pScriptComp->m_ScriptName = "CPersistentTrackerScript";
    persistentParent.GetComponents()->AddComponent(pScriptComp);
    level1.m_vEntities.push_back(std::move(persistentParent));

    CEntity persistentChild;
    persistentChild.m_Id = 1;
    persistentChild.m_ParentId = 0;
    persistentChild.m_Name = "PersistentChild";
    level1.m_vEntities.push_back(std::move(persistentChild));

    CEntity transientObj;
    transientObj.m_Id = 2;
    transientObj.m_Name = "Level1Only";
    level1.m_vEntities.push_back(std::move(transientObj));

    const fs::path level1Path = tempRoot / "level1.json";
    {
        std::ofstream out(level1Path);
        out << quark::CSceneDocument::Serialize(level1);
    }

    CScene level2;
    CEntity level2Obj;
    level2Obj.m_Id = 0;
    level2Obj.m_Name = "Level2Only";
    level2.m_vEntities.push_back(std::move(level2Obj));

    const fs::path level2Path = tempRoot / "resources" / "level2.json";
    {
        std::ofstream out(level2Path);
        out << quark::CSceneDocument::Serialize(level2);
    }

    CScriptEngine& engine = CScriptEngine::Get();
    int hostLoadedCount = 0;
    engine.StartPlay(
        level1,
        tempRoot.string(),
        nullptr,
        level1Path.string(),
        [&hostLoadedCount](CScene&)
        {
            ++hostLoadedCount;
        });

    CHECK(SceneManager::GetActiveSceneName() == "level1");
    CHECK(SceneManager::GetSceneCount() == 2);
    CHECK(CPersistentTrackerScript::ms_StartCount == 1);

    SceneManager::SetString("player_name", "Hero");
    SceneManager::SetInt("score", 420);
    SceneManager::SetFloat("health", 87.5f);
    SceneManager::SetBool("has_key", true);
    SceneManager::SetInt("temp_key", 99);
    CHECK(SceneManager::HasKey("temp_key"));
    SceneManager::RemoveKey("temp_key");
    CHECK(!SceneManager::HasKey("temp_key"));

    CPersistentTrackerScript::ms_TargetScene = "level2";
    CPersistentTrackerScript::ms_RequestLoadOnUpdate = true;
    engine.UpdatePlay(level1, 0.016f);

    CHECK(hostLoadedCount == 1);
    CHECK(SceneManager::GetActiveSceneName() == "level2");
    CHECK(Scene::FindEntityByName("Level2Only") != nullptr);
    CHECK(Scene::FindEntityByName("Level1Only") == nullptr);
    CHECK(Scene::FindEntityByName("PersistentManager") != nullptr);
    CHECK(Scene::FindEntityByName("PersistentChild") != nullptr);
    CHECK(CPersistentTrackerScript::ms_StartCount == 1);
    CHECK(CPersistentTrackerScript::ms_DestroyCount == 0);

    CScriptEntity* pSurvivingParent = Scene::FindEntityByName("PersistentManager");
    CScriptEntity* pSurvivingChild = Scene::FindEntityByName("PersistentChild");
    CHECK(pSurvivingParent != nullptr);
    CHECK(pSurvivingChild != nullptr);
    const int childIdx = level1.FindEntityIndexById(pSurvivingChild->GetId());
    CHECK(childIdx >= 0);
    CHECK(level1.m_vEntities[static_cast<size_t>(childIdx)].m_ParentId == pSurvivingParent->GetId());

    CHECK(SceneManager::GetString("player_name") == "Hero");
    CHECK(SceneManager::GetInt("score") == 420);
    CHECK_NEAR(SceneManager::GetFloat("health"), 87.5f, 0.001f);
    CHECK(SceneManager::GetBool("has_key") == true);

    CHECK(SceneManager::ReloadScene());
    CHECK(hostLoadedCount == 2);
    CHECK(SceneManager::GetActiveSceneName() == "level2");
    CHECK(Scene::FindEntityByName("PersistentManager") != nullptr);
    CHECK(CPersistentTrackerScript::ms_DestroyCount == 0);

    engine.StopPlay(level1);
    CHECK(!SceneManager::HasKey("player_name"));
    CHECK(CPersistentTrackerScript::ms_DestroyCount == 1);

    fs::remove_all(tempRoot, ec);
}

namespace
{
    class CHealthDemoScript : public IScript
    {
    public:
        float m_Speed = 5.0f;
        int m_Health = 100;
        bool m_Invulnerable = false;
        std::string m_Title = "Default";
        Vec3 m_Offset = { 1.0f, 2.0f, 3.0f };
        Color m_Tint = WHITE;
        EntityRef m_Target;
        AssetRef m_Asset;
        int m_InvokeHits = 0;
        int m_RepeatHits = 0;
        std::string m_LastEventPayload;
        std::uint64_t m_RepeatTimerId = 0;
        std::uint64_t m_SubId = 0;

        QUARK_PROPERTIES(
            QUARK_FIELD(m_Speed),
            QUARK_FIELD(m_Health),
            QUARK_FIELD(m_Invulnerable),
            QUARK_FIELD(m_Title),
            QUARK_FIELD(m_Offset),
            QUARK_FIELD(m_Tint),
            QUARK_FIELD(m_Target),
            QUARK_FIELD(m_Asset)
        )

        void TakeDamage(int amount)
        {
            m_Health -= amount;
        }

        void OnStart() override
        {
            m_SubId = EventBus::Subscribe("OnHeal", [this](const std::string& payload)
            {
                m_LastEventPayload = payload;
                m_Health += 25;
            });
            Invoke(0.05f, [this]()
            {
                ++m_InvokeHits;
            });
            m_RepeatTimerId = InvokeRepeating(0.04f, 0.04f, [this]()
            {
                ++m_RepeatHits;
            });
        }
    };
} // anonymous

TEST(ScriptSDK, PropertiesEventBusTimersUiCameraDebugAndHierarchy)
{
    CHealthDemoScript propTest;
    propTest.SetPropertyFloat("m_Speed", 12.5f);
    propTest.SetPropertyInt("Health", 75);
    propTest.SetPropertyBool("m_Invulnerable", true);
    propTest.SetPropertyString("Title", "Boss");
    propTest.SetPropertyVec3("m_Offset", { 4.0f, 5.0f, 6.0f });
    propTest.SetPropertyColor("Tint", RED);
    propTest.SetPropertyEntityRef("m_Target", EntityRef(42));
    propTest.SetPropertyAssetRef("Asset", AssetRef("models/hero.glb"));

    float outSpeed = 0.0f;
    int outHealth = 0;
    bool outInvuln = false;
    std::string outTitle;
    Vec3 outOffset{};
    Color outTint{};
    EntityRef outTarget;
    AssetRef outAsset;
    CHECK(propTest.GetPropertyFloat("Speed", outSpeed) && std::fabs(outSpeed - 12.5f) < 0.001f);
    CHECK(propTest.GetPropertyInt("m_Health", outHealth) && outHealth == 75);
    CHECK(propTest.GetPropertyBool("Invulnerable", outInvuln) && outInvuln);
    CHECK(propTest.GetPropertyString("m_Title", outTitle) && outTitle == "Boss");
    CHECK(propTest.GetPropertyVec3("Offset", outOffset) && std::fabs(outOffset.y - 5.0f) < 0.001f);
    CHECK(propTest.GetPropertyColor("m_Tint", outTint) && outTint.r == RED.r);
    CHECK(propTest.GetPropertyEntityRef("Target", outTarget) && outTarget.m_EntityId == 42);
    CHECK(propTest.GetPropertyAssetRef("m_Asset", outAsset) && outAsset.m_Path == "models/hero.glb");

    auto& registry = quark::script::CScriptRegistry::Get();
    registry.Register("CHealthDemoScript",
                      []() -> IScript* { return new CHealthDemoScript(); },
                      [](IScript* p) { delete p; });

    CScene scene;
    CEntity parent;
    parent.m_Id = 0;
    parent.m_Name = "ParentRig";
    parent.GetTransformComponent()->m_Position = { 10.0f, 2.0f, 0.0f };
    auto pScriptComp = std::make_shared<CScriptComponent>();
    pScriptComp->m_ScriptName = "CHealthDemoScript";
    parent.GetComponents()->AddComponent(pScriptComp);
    scene.m_vEntities.push_back(std::move(parent));

    CEntity child;
    child.m_Id = 1;
    child.m_Name = "ChildNode";
    child.m_ParentId = -1;
    child.GetTransformComponent()->m_Position = { 3.0f, 0.0f, 0.0f };
    child.GetComponents()->AddComponent(std::make_shared<CMeshComponent>());
    scene.m_vEntities.push_back(std::move(child));

    CEntity camEnt;
    camEnt.m_Id = 2;
    camEnt.m_Name = "MainCamera";
    camEnt.GetTransformComponent()->m_Position = { 0.0f, 1.0f, 5.0f };
    auto pCamComp = std::make_shared<CCameraComponent>();
    pCamComp->m_FieldOfView = 60.0f;
    camEnt.GetComponents()->AddComponent(pCamComp);
    scene.m_vEntities.push_back(std::move(camEnt));

    CEntity uiEnt;
    uiEnt.m_Id = 3;
    uiEnt.m_Name = "HudLabel";
    auto pTextComp = std::make_shared<CTextComponent>();
    pTextComp->m_Text = "HP: 100";
    auto pProgComp = std::make_shared<CProgressBarComponent>();
    pProgComp->m_Value = 1.0f;
    auto pSliderComp = std::make_shared<CSliderComponent>();
    pSliderComp->m_Value = 0.5f;
    auto pToggleComp = std::make_shared<CToggleComponent>();
    pToggleComp->m_IsOn = false;
    uiEnt.GetComponents()->AddComponent(pTextComp);
    uiEnt.GetComponents()->AddComponent(pProgComp);
    uiEnt.GetComponents()->AddComponent(pSliderComp);
    uiEnt.GetComponents()->AddComponent(pToggleComp);
    scene.m_vEntities.push_back(std::move(uiEnt));

    auto& engine = quark::script::CScriptEngine::Get();
    engine.SetActiveViewportRect({ 0.0f, 0.0f }, { 800.0f, 600.0f });
    engine.StartPlay(scene, "");

    CScriptEntity* pParent = Scene::FindEntityByName("ParentRig");
    CScriptEntity* pChild = Scene::FindEntityByName("ChildNode");
    CHECK(pParent != nullptr && pChild != nullptr);

    CHealthDemoScript* pDemo = pParent->GetScript<CHealthDemoScript>();
    CHECK(pDemo != nullptr);
    pDemo->TakeDamage(20);
    CHECK(pDemo->m_Health == 80);

    EventBus::Emit("OnHeal", "medkit");
    CHECK(pDemo->m_Health == 105);
    CHECK(pDemo->m_LastEventPayload == "medkit");
    EventBus::Unsubscribe(pDemo->m_SubId);
    EventBus::Emit("OnHeal", "ignored");
    CHECK(pDemo->m_Health == 105);

    engine.UpdatePlay(scene, 0.06f);
    CHECK(pDemo->m_InvokeHits == 1);
    CHECK(pDemo->m_RepeatHits == 1);
    engine.UpdatePlay(scene, 0.05f);
    CHECK(pDemo->m_InvokeHits == 1);
    CHECK(pDemo->m_RepeatHits == 2);
    pDemo->CancelInvoke(pDemo->m_RepeatTimerId);
    engine.UpdatePlay(scene, 0.1f);
    CHECK(pDemo->m_RepeatHits == 2);

    pChild->SetParent(pParent, false);
    CHECK(pChild->GetParent() == pParent);
    const std::vector<CScriptEntity*> vChildren = pParent->GetChildren();
    CHECK(vChildren.size() == 1 && vChildren[0] == pChild);
    const Vec3 childWorld = pChild->GetWorldPosition();
    CHECK_NEAR(childWorld.x, 13.0f, 0.01f);
    CHECK_NEAR(childWorld.y, 2.0f, 0.01f);
    pChild->SetActive(false);
    CHECK(!pChild->IsActive());
    pChild->SetActive(true);
    CHECK(pChild->IsActive());

    CScriptEntity* pHud = Scene::FindEntityByName("HudLabel");
    CHECK(pHud != nullptr);
    UI::SetText("HudLabel", "HP: 80");
    CHECK(UI::GetText(pHud) == "HP: 80");
    UI::SetProgress("HudLabel", 0.42f);
    CHECK_NEAR(UI::GetProgress(pHud), 0.42f, 0.001f);
    UI::SetSliderValue(pHud, 0.75f);
    CHECK_NEAR(UI::GetSliderValue(pHud), 0.75f, 0.001f);
    UI::SetToggleValue(pHud, true);
    CHECK(UI::GetToggleValue(pHud) == true);
    UI::SetColor(pHud, GREEN);
    UI::SetVisible("HudLabel", false);
    CHECK(!UI::IsVisible(pHud));

    CScriptEntity* pCam = CameraAPI::GetMainCamera();
    CHECK(pCam != nullptr && pCam->GetName() == "MainCamera");
    CameraAPI::SetFov(pCam, 75.0f);
    CHECK_NEAR(CameraAPI::GetFov(pCam), 75.0f, 0.001f);
    const SRay centerRay = CameraAPI::ScreenPointToRay({ 400.0f, 300.0f });
    CHECK_NEAR(centerRay.m_Origin.z, 5.0f, 0.01f);
    CHECK_NEAR(centerRay.m_Direction.z, -1.0f, 0.01f);

    Debug::DrawLine({ 0, 0, 0 }, { 1, 0, 0 }, RED, 0.5f);
    Debug::DrawRay({ 0, 0, 0 }, { 0, 1, 0 }, GREEN, 0.0f);
    Debug::DrawWireBox({ 0, 1, 0 }, { 2, 2, 2 }, YELLOW, 0.5f);
    Debug::DrawWireSphere({ 0, 0, 0 }, 1.0f, SKYBLUE, 0.5f);
    CHECK(engine.GetDebugDrawCount() == 4);
    engine.UpdatePlay(scene, 0.1f);
    CHECK(engine.GetDebugDrawCount() == 3);

    Application::Quit();
    CHECK(engine.ConsumeQuitRequested() == true);
    CHECK(engine.ConsumeQuitRequested() == false);

    engine.StopPlay(scene);
    CHECK(engine.GetDebugDrawCount() == 0);
}
