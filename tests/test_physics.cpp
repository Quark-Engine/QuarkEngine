#include "test_harness.h"

#include "QuarkEngine/SDK.h"
#include "engine/physics_world.h"
#include "scene.h"
#include "scripting/script_engine.h"

#include <cmath>

namespace
{
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
