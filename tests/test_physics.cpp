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
    original.m_Friction = 0.7f;
    original.m_Restitution = 0.4f;
    original.m_UseGravity = false;
    original.m_IsKinematic = true;
    original.m_FreezePositionX = true;
    original.m_FreezePositionY = false;
    original.m_FreezePositionZ = true;

    nlohmann::json data;
    original.Serialize(data);

    CRigidbodyComponent restored;
    restored.Deserialize(data);

    CHECK_NEAR(restored.m_Mass, 4.5f, 0.001f);
    CHECK_NEAR(restored.m_LinearDrag, 0.25f, 0.001f);
    CHECK_NEAR(restored.m_Friction, 0.7f, 0.001f);
    CHECK_NEAR(restored.m_Restitution, 0.4f, 0.001f);
    CHECK(!restored.m_UseGravity);
    CHECK(restored.m_IsKinematic);
    CHECK(restored.m_FreezePositionX);
    CHECK(!restored.m_FreezePositionY);
    CHECK(restored.m_FreezePositionZ);
}
