#include "test_harness.h"

#include "engine/scene_document.h"

#include "QuarkCore/QuarkCore.hpp"

#include <algorithm>
#include <stack>
#include <string>
#include <vector>

using namespace qc;

using qc::Vec3;

namespace
{

int AddEntity(CScene& target, const std::string& name, int parentId, bool isGroup = false)
{
    CEntity added(static_cast<int>(target.m_vEntities.size()));
    added.m_Name = name;
    added.m_ParentId = parentId;
    added.m_IsGroup = isGroup;
    target.m_vEntities.push_back(added);
    return static_cast<int>(target.m_vEntities.size()) - 1;
}

void ExpectVecNear(const Vec3& actual, const Vec3& expected, double eps)
{
    CHECK_NEAR(actual.x, expected.x, eps);
    CHECK_NEAR(actual.y, expected.y, eps);
    CHECK_NEAR(actual.z, expected.z, eps);
}

} // anonymous

TEST(CSceneDocumentSerialize, empty_scene_produces_a_valid_document)
{
    CComponentFactoryRegistry registry;
    CScene sceneUnderTest;
    const std::string document = quark::CSceneDocument::Serialize(sceneUnderTest);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(document, restored, registry));
    CHECK(restored.m_vEntities.empty());
}

TEST(CSceneDocumentDeserialize, rejects_malformed_documents_without_touching_the_scene)
{
    CComponentFactoryRegistry registry;
    CScene sceneUnderTest;
    AddEntity(sceneUnderTest, "keep", -1);

    CHECK(!quark::CSceneDocument::Deserialize("", sceneUnderTest, registry));
    CHECK(!quark::CSceneDocument::Deserialize("not json at all {", sceneUnderTest, registry));
    CHECK(!quark::CSceneDocument::Deserialize("[]", sceneUnderTest, registry));
    CHECK(!quark::CSceneDocument::Deserialize("{\"version\":\"1\"}", sceneUnderTest, registry));

    CHECK(sceneUnderTest.m_vEntities.size() == 1);
    CHECK(sceneUnderTest.m_vEntities[0].m_Name == "keep");
}

TEST(CSceneDocumentParse, parses_snapshots_for_worker_thread_deserialization)
{
    CScene source;
    AddEntity(source, "worker-parsed", -1);
    const std::string document = quark::CSceneDocument::Serialize(source, 0);
    const quark::SParsedSceneDocument parsed = quark::CSceneDocument::Parse(document);

    CHECK(parsed.IsValid);
    CComponentFactoryRegistry registry;
    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(parsed, restored, registry));
    CHECK(restored.m_vEntities.size() == 1);
    CHECK(restored.m_vEntities[0].m_Name == "worker-parsed");
}

TEST(CSceneDocumentParse, rejects_invalid_documents)
{
    CHECK(!quark::CSceneDocument::Parse("not json").IsValid);
    CHECK(!quark::CSceneDocument::Parse("{\"version\":\"1\"}").IsValid);
}

TEST(SceneSnapshot, round_trips_entity_identity_and_hierarchy)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int root = AddEntity(source, "root", -1);
    AddEntity(source, "child", root);
    AddEntity(source, "group", -1, true);

    const std::string document = quark::CSceneDocument::Serialize(source);
    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(document, restored, registry));

    CHECK(restored.m_vEntities.size() == 3);
    CHECK(restored.m_vEntities[0].m_Name == "root");
    CHECK(restored.m_vEntities[1].m_Name == "child");
    CHECK(restored.m_vEntities[1].m_ParentId == 0);
    CHECK(restored.m_vEntities[2].m_Name == "group");
    CHECK(restored.m_vEntities[2].m_IsGroup);
}

TEST(SceneSnapshot, round_trips_tags)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "tagged", -1);
    source.m_vEntities[index].m_vTags = { "alpha", "beta" };

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    CHECK(restored.m_vEntities[0].m_vTags.size() == 2);
    CHECK(restored.m_vEntities[0].m_vTags[0] == "alpha");
    CHECK(restored.m_vEntities[0].m_vTags[1] == "beta");
}

TEST(SceneSnapshot, round_trips_transform)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "moved", -1);
    CTransformComponent* pTransform = source.m_vEntities[index].GetTransformComponent();
    pTransform->m_Position = { 1.5f, -2.0f, 3.25f };
    pTransform->m_Rotation = { 15.0f, 30.0f, 45.0f };
    pTransform->m_Scale = { 2.0f, 0.5f, 1.5f };

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    const CTransformComponent* pRestored = restored.m_vEntities[0].GetTransformComponent();
    CHECK(pRestored != nullptr);
    ExpectVecNear(pRestored->m_Position, Vec3{ 1.5f, -2.0f, 3.25f }, 1e-6);
    ExpectVecNear(pRestored->m_Rotation, Vec3{ 15.0f, 30.0f, 45.0f }, 1e-6);
    ExpectVecNear(pRestored->m_Scale, Vec3{ 2.0f, 0.5f, 1.5f }, 1e-6);
}

TEST(SceneSnapshot, round_trips_collision_component)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "collider", -1);

    CComponentManager* pComponents = source.m_vEntities[index].GetComponents();
    auto pCollision = std::make_shared<CCollisionComponent>();
    pCollision->m_ColliderType = COLLIDER_CAPSULE;
    pCollision->m_IsTrigger = true;
    pCollision->m_Size = { 1.0f, 2.0f, 3.0f };
    pCollision->m_Radius = 0.75f;
    pCollision->m_Height = 4.5f;
    pCollision->m_Center = { 0.5f, 1.5f, 2.5f };
    pComponents->AddComponent(pCollision);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    const CCollisionComponent* pRestored = restored.m_vEntities[0].GetCollisionComponent();
    CHECK_MSG(pRestored != nullptr, "collision component was dropped by the round-trip");
    if (!pRestored)
    {
        return;
    }
    CHECK(pRestored->m_ColliderType == COLLIDER_CAPSULE);
    CHECK(pRestored->m_IsTrigger);
    CHECK_NEAR(pRestored->m_Size.x, 1.0f, 1e-6);
    CHECK_NEAR(pRestored->m_Size.y, 2.0f, 1e-6);
    CHECK_NEAR(pRestored->m_Size.z, 3.0f, 1e-6);
    CHECK_NEAR(pRestored->m_Radius, 0.75f, 1e-6);
    CHECK_NEAR(pRestored->m_Height, 4.5f, 1e-6);
    CHECK_NEAR(pRestored->m_Center.x, 0.5f, 1e-6);
    CHECK_NEAR(pRestored->m_Center.y, 1.5f, 1e-6);
    CHECK_NEAR(pRestored->m_Center.z, 2.5f, 1e-6);
}

TEST(SceneSnapshot, round_trips_material_component)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "textured", -1);

    CComponentManager* pComponents = source.m_vEntities[index].GetComponents();
    auto pMaterial = std::make_shared<CMaterialComponent>();
    pMaterial->m_Color = { 10, 20, 30, 40 };
    pMaterial->m_OutlineColor = { 50, 60, 70, 80 };
    pMaterial->m_TextureStretch = false;
    pMaterial->m_TextureRepeatU = 2.5f;
    pMaterial->m_TextureRepeatV = 3.5f;
    pMaterial->m_UvScale = { 1.25f, 0.75f };
    pMaterial->m_AlbedoTextureName = "albedo.png";
    pComponents->AddComponent(pMaterial);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    const CMaterialComponent* pRestored = restored.m_vEntities[0].GetMaterialComponent();
    CHECK(pRestored != nullptr);
    if (!pRestored)
    {
        return;
    }
    CHECK(pRestored->m_Color.r == 10);
    CHECK(pRestored->m_Color.g == 20);
    CHECK(pRestored->m_Color.b == 30);
    CHECK(pRestored->m_Color.a == 40);
    CHECK(pRestored->m_OutlineColor.r == 50);
    CHECK(!pRestored->m_TextureStretch);
    CHECK_NEAR(pRestored->m_TextureRepeatU, 2.5f, 1e-6);
    CHECK_NEAR(pRestored->m_TextureRepeatV, 3.5f, 1e-6);
    CHECK_NEAR(pRestored->m_UvScale.x, 1.25f, 1e-6);
    CHECK_NEAR(pRestored->m_UvScale.y, 0.75f, 1e-6);
    CHECK(pRestored->m_AlbedoTextureName == "albedo.png");
}

TEST(SceneSnapshot, round_trips_light_component)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "light", -1);

    CComponentManager* pComponents = source.m_vEntities[index].GetComponents();
    auto pLight = std::make_shared<CLightComponent>();
    pLight->m_Light = CreateLighting({ 1.0f, 2.0f, 3.0f }, RED);
    pLight->m_Light.m_Intensity = 2.5f;
    pLight->m_Light.m_Range = 42.0f;
    pLight->m_Light.m_Light.type = LIGHT_POINT;
    pComponents->AddComponent(pLight);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    const CLightComponent* pRestored = restored.m_vEntities[0].GetLightComponent();
    CHECK(pRestored != nullptr);
    if (!pRestored)
    {
        return;
    }
    CHECK_NEAR(pRestored->m_Light.m_Intensity, 2.5f, 1e-4);
    CHECK_NEAR(pRestored->m_Light.m_Range, 42.0f, 1e-4);
    CHECK(pRestored->m_Light.m_Color.r == RED.r);
    CHECK(pRestored->m_Light.m_Color.g == RED.g);
    CHECK(pRestored->m_Light.m_Color.b == RED.b);
}

TEST(SceneSnapshot, round_trips_editable_mesh_vertices)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "editable", -1);

    CMeshComponent* pMesh = source.m_vEntities[index].GetMeshComponent();
    pMesh->m_IsEditableMesh = true;
    pMesh->m_AssetName = "some.obj";
    SEditableVertex vertex;
    vertex.Position = { 0.25f, 0.5f, 0.75f };
    vertex.U = 0.125f;
    vertex.V = 0.875f;
    pMesh->m_EditableMesh.m_vVertices.push_back(vertex);
    pMesh->m_EditableMesh.m_vTriangles.push_back({ 0, 0, 0 });

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    const CMeshComponent* pRestored = restored.m_vEntities[0].GetMeshComponent();
    CHECK(pRestored != nullptr);
    if (!pRestored)
    {
        return;
    }
    CHECK(pRestored->m_IsEditableMesh);
    CHECK(pRestored->m_AssetName == "some.obj");
    CHECK(pRestored->m_EditableMesh.m_vVertices.size() == 1);
    CHECK(pRestored->m_EditableMesh.m_vTriangles.size() == 1);
    ExpectVecNear(pRestored->m_EditableMesh.m_vVertices[0].Position, Vec3{ 0.25f, 0.5f, 0.75f }, 1e-6);
    CHECK_NEAR(pRestored->m_EditableMesh.m_vVertices[0].U, 0.125f, 1e-6);
    CHECK_NEAR(pRestored->m_EditableMesh.m_vVertices[0].V, 0.875f, 1e-6);
}

TEST(SceneSnapshot, keeps_entities_that_have_no_mesh)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "bare", -1);

    CComponentManager* pComponents = source.m_vEntities[index].GetComponents();
    for (size_t i = 0; i < pComponents->GetComponentCount(); ++i)
    {
        if (pComponents->GetComponent(i)->GetType() == COMPONENT_MESH)
        {
            pComponents->RemoveComponent(i);
            break;
        }
    }

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    CHECK_MSG(restored.m_vEntities.size() == 1, "a mesh-less entity was dropped by the round-trip");
    if (restored.m_vEntities.empty())
    {
        return;
    }
    CHECK(restored.m_vEntities[0].m_Name == "bare");
    CHECK(restored.m_vEntities[0].GetMeshComponent() == nullptr);
    CHECK(restored.m_vEntities[0].GetTransformComponent() != nullptr);
}

TEST(SceneSnapshot, reassigns_ids_to_match_the_entity_index)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int root = AddEntity(source, "root", -1);
    AddEntity(source, "child", root);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    CHECK(restored.m_vEntities[0].m_Id == 0);
    CHECK(restored.m_vEntities[1].m_Id == 1);
}

TEST(CSceneDocumentCaptureSnapshot, carries_selection_alongside_the_document)
{
    CScene sceneUnderTest;
    AddEntity(sceneUnderTest, "a", -1);
    AddEntity(sceneUnderTest, "b", -1);
    sceneUnderTest.m_Selected = 1;
    sceneUnderTest.m_vSelectedEntities = { 0, 1 };

    const quark::SSceneSnapshot snapshot = quark::CSceneDocument::CaptureSnapshot(sceneUnderTest);
    CHECK(snapshot.Selected == 1);
    CHECK(snapshot.vSelectedEntities.size() == 2);
    CHECK(snapshot.vSelectedEntities[0] == 0);
    CHECK(snapshot.vSelectedEntities[1] == 1);
    CHECK(!snapshot.Document.empty());
}

TEST(CSceneDocumentCaptureSnapshot, document_restores_into_an_identical_scene)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int root = AddEntity(source, "root", -1);
    const int child = AddEntity(source, "child", root);
    source.m_vEntities[child].m_vTags = { "keep", "me" };
    CTransformComponent* pTransform = source.m_vEntities[child].GetTransformComponent();
    pTransform->m_Position = { 9.0f, 8.0f, 7.0f };
    pTransform->m_Rotation = { 1.0f, 2.0f, 3.0f };
    pTransform->m_Scale = { 4.0f, 5.0f, 6.0f };
    source.m_Selected = child;
    source.m_vSelectedEntities = { root, child };

    const quark::SSceneSnapshot before = quark::CSceneDocument::CaptureSnapshot(source);

    source.m_vEntities[child].GetTransformComponent()->m_Position = { -1.0f, 0.0f, 0.0f };
    source.m_vEntities[child].m_vTags.clear();
    source.m_vEntities.pop_back();
    source.m_Selected = 5;
    source.m_vSelectedEntities = { 7 };

    CHECK(quark::CSceneDocument::Deserialize(before.Document, source, registry));
    source.m_Selected = before.Selected;
    source.m_vSelectedEntities = before.vSelectedEntities;

    CHECK(source.m_vEntities.size() == 2);
    CHECK(source.m_vEntities[child].m_Name == "child");
    CHECK(source.m_vEntities[child].m_vTags.size() == 2);
    ExpectVecNear(source.m_vEntities[child].GetTransformComponent()->m_Position, Vec3
    { 9.0f, 8.0f, 7.0f }, 1e-6);
    ExpectVecNear(source.m_vEntities[child].GetTransformComponent()->m_Rotation, Vec3
    { 1.0f, 2.0f, 3.0f }, 1e-6);
    ExpectVecNear(source.m_vEntities[child].GetTransformComponent()->m_Scale, Vec3
    { 4.0f, 5.0f, 6.0f }, 1e-6);
    CHECK(source.m_Selected == child);
    CHECK(source.m_vSelectedEntities.size() == 2);
    CHECK(source.m_vSelectedEntities[0] == root);
    CHECK(source.m_vSelectedEntities[1] == child);
}

namespace
{

class SSnapshotHistory
{
public:
    explicit SSnapshotHistory(CScene& scene, const CComponentFactoryRegistry& registries)
        : m_Scene(scene)
        , m_Registry(registries)
    {
    }

    void Save()
    {
        m_Undo.push(quark::CSceneDocument::CaptureSnapshot(m_Scene));
        while (!m_Redo.empty())
        {
            m_Redo.pop();
        }
    }

    bool Undo()
    {
        if (m_Undo.empty())
        {
            return false;
        }
        const quark::SSceneSnapshot previous = m_Undo.top();
        m_Undo.pop();
        m_Redo.push(quark::CSceneDocument::CaptureSnapshot(m_Scene));
        Restore(previous);
        return true;
    }

    bool Redo()
    {
        if (m_Redo.empty())
        {
            return false;
        }
        const quark::SSceneSnapshot next = m_Redo.top();
        m_Redo.pop();
        m_Undo.push(quark::CSceneDocument::CaptureSnapshot(m_Scene));
        Restore(next);
        return true;
    }

private:
    void Restore(const quark::SSceneSnapshot& snapshot)
    {
        CHECK(quark::CSceneDocument::Deserialize(snapshot.Document, m_Scene, m_Registry));
        m_Scene.m_Selected = snapshot.Selected;
        m_Scene.m_vSelectedEntities = snapshot.vSelectedEntities;
    }

    CScene& m_Scene;
    const CComponentFactoryRegistry& m_Registry;
    std::stack<quark::SSceneSnapshot> m_Undo;
    std::stack<quark::SSceneSnapshot> m_Redo;
};

int CountEntities(const CScene& scene)
{
    return static_cast<int>(scene.m_vEntities.size());
}

std::string NameAt(const CScene& scene, int index)
{
    return scene.m_vEntities[index].m_Name;
}

} // anonymous

TEST(SnapshotHistory, undo_and_redo_walk_a_multi_edit_session_backwards_and_forwards)
{
    CScene scene;
    CComponentFactoryRegistry registry;
    SSnapshotHistory history(scene, registry);

    AddEntity(scene, "before", -1);
    history.Save();
    AddEntity(scene, "added", -1);
    history.Save();
    scene.m_vEntities[0].m_Name = "renamed";
    history.Save();
    scene.m_vEntities.pop_back();
    CHECK(CountEntities(scene) == 1);
    CHECK(NameAt(scene, 0) == "renamed");

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 2);
    CHECK(NameAt(scene, 0) == "renamed");
    CHECK(NameAt(scene, 1) == "added");

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 2);
    CHECK(NameAt(scene, 0) == "before");

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 1);
    CHECK(NameAt(scene, 0) == "before");

    CHECK(!history.Undo());
    CHECK(CountEntities(scene) == 1);

    CHECK(history.Redo());
    CHECK(CountEntities(scene) == 2);
    CHECK(NameAt(scene, 0) == "before");

    CHECK(history.Redo());
    CHECK(CountEntities(scene) == 2);
    CHECK(NameAt(scene, 0) == "renamed");

    CHECK(history.Redo());
    CHECK(CountEntities(scene) == 1);
    CHECK(NameAt(scene, 0) == "renamed");
}

TEST(SnapshotHistory, saving_after_undo_drops_the_redo_branch)
{
    CScene scene;
    CComponentFactoryRegistry registry;
    SSnapshotHistory history(scene, registry);

    AddEntity(scene, "one", -1);
    history.Save();
    AddEntity(scene, "two", -1);
    history.Save();
    AddEntity(scene, "three", -1);
    CHECK(CountEntities(scene) == 3);

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 2);

    history.Save();
    CHECK(!history.Redo());
    CHECK(CountEntities(scene) == 2);
}

TEST(SnapshotHistory, undo_and_redo_on_empty_history_are_noops)
{
    CScene scene;
    CComponentFactoryRegistry registry;
    SSnapshotHistory history(scene, registry);

    CHECK(!history.Undo());
    CHECK(!history.Redo());
    CHECK(scene.m_vEntities.empty());

    AddEntity(scene, "one", -1);
    history.Save();
    AddEntity(scene, "two", -1);
    CHECK(CountEntities(scene) == 2);

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 1);
    CHECK(NameAt(scene, 0) == "one");

    CHECK(!history.Undo());
    CHECK(CountEntities(scene) == 1);

    CHECK(history.Redo());
    CHECK(CountEntities(scene) == 2);
    CHECK(NameAt(scene, 1) == "two");

    CHECK(!history.Redo());
    CHECK(CountEntities(scene) == 2);
}

namespace
{

class CTestGaugeComponent : public IComponent
{
public:
    int m_Reading = 0;
    std::string m_Label;

    CTestGaugeComponent() = default;

    std::string GetTypeName() const override
    {
        return "Test Gauge";
    }
    EComponentType GetType() const override
    {
        return COMPONENT_CUSTOM;
    }

    void Serialize(nlohmann::json& json) const override
    {
        json["reading"] = m_Reading;
        json["label"] = m_Label;
    }

    void Deserialize(const nlohmann::json& json) override
    {
        if (json.contains("reading")) m_Reading = json["reading"];
        if (json.contains("label")) m_Label = json["label"];
    }
};

const char* const s_GaugeTypeName = "Test Gauge";

} // anonymous

TEST(ComponentFactory, registered_custom_component_survives_a_snapshot_round_trip)
{
    CComponentFactoryRegistry registry;
    registry.Register(s_GaugeTypeName, []()
    {
        return std::make_shared<CTestGaugeComponent>();
    });

    CScene source;
    const int index = AddEntity(source, "gauged", -1);
    auto pGauge = std::make_shared<CTestGaugeComponent>();
    pGauge->m_Reading = 42;
    pGauge->m_Label = "pressure";
    pGauge->m_Enabled = false;
    source.m_vEntities[index].m_pComponents->AddComponent(pGauge);

    const quark::SSceneSnapshot before = quark::CSceneDocument::CaptureSnapshot(source);

    source.m_vEntities.clear();

    CHECK(quark::CSceneDocument::Deserialize(before.Document, source, registry));
    CHECK(source.m_vEntities.size() == 1);

    const auto pRestored = source.m_vEntities[0].m_pComponents->GetComponentOfType<CTestGaugeComponent>();
    CHECK(pRestored != nullptr);
    if (pRestored)
    {
        CHECK(pRestored->m_Reading == 42);
        CHECK(pRestored->m_Label == "pressure");
        CHECK(pRestored->m_Enabled == false);
    }

    registry.Unregister(s_GaugeTypeName);
}

TEST(ComponentFactory, unregistered_custom_component_is_dropped_without_losing_the_entity)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "gauged", -1);
    auto pGauge = std::make_shared<CTestGaugeComponent>();
    pGauge->m_Reading = 7;
    source.m_vEntities[index].m_pComponents->AddComponent(pGauge);

    const std::string document = quark::CSceneDocument::Serialize(source);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(document, restored, registry));
    CHECK(restored.m_vEntities.size() == 1);
    CHECK(restored.m_vEntities[0].m_pComponents->GetComponentOfType<CTestGaugeComponent>() == nullptr);
    CHECK(restored.m_vEntities[0].m_pComponents->GetTransform() != nullptr);
}

TEST(ComponentFactory, built_in_type_names_keep_resolving_alongside_plugin_registrations)
{
    CComponentFactoryRegistry registry;
    registry.Register(s_GaugeTypeName, []()
    {
        return std::make_shared<CTestGaugeComponent>();
    });

    CHECK(registry.Create("Transform") != nullptr);
    CHECK(registry.Create("Mesh") != nullptr);
    CHECK(registry.Create("Material") != nullptr);
    CHECK(registry.Create("Light") != nullptr);
    CHECK(registry.Create("Collision") != nullptr);
    CHECK(registry.Create("Text") != nullptr);
    CHECK(registry.Create("3D Text") != nullptr);
    CHECK(registry.Create("Test Gauge") != nullptr);
    CHECK(registry.Create("Not A Real Component") == nullptr);

    registry.Unregister(s_GaugeTypeName);
    CHECK(registry.Create("Test Gauge") == nullptr);
    CHECK(registry.Create("Transform") != nullptr);
}

TEST(ComponentFactory, empty_registrations_are_ignored)
{
    CComponentFactoryRegistry registry;
    CHECK(!registry.Register("", []()
    {
        return std::make_shared<CTestGaugeComponent>();
    }));
    CHECK(!registry.Register("Null Factory", nullptr));
    CHECK(registry.Create("") == nullptr);
    CHECK(registry.Create("Null Factory") == nullptr);
}

TEST(ComponentFactory, built_in_type_names_cannot_be_replaced_or_unregistered)
{
    CComponentFactoryRegistry registry;
    const auto stolen = []()
    {
        return std::make_shared<CTestGaugeComponent>();
    };
    CHECK(!registry.Register("Transform", stolen));
    CHECK(!registry.Register("3D Text", stolen));

    const auto pTransform = registry.Create("Transform");
    CHECK(pTransform != nullptr);
    CHECK(pTransform->GetTypeName() == "Transform");

    registry.Unregister("Transform");
    CHECK(registry.Create("Transform") != nullptr);
}

TEST(ComponentFactory, IsBuiltIn_separates_reserved_type_names_from_registered_ones)
{
    CComponentFactoryRegistry registry;
    registry.Register(s_GaugeTypeName, []()
    {
        return std::make_shared<CTestGaugeComponent>();
    });

    CHECK(registry.IsBuiltIn("Transform"));
    CHECK(registry.IsBuiltIn("3D Text"));
    CHECK(!registry.IsBuiltIn(s_GaugeTypeName));
    CHECK(!registry.IsBuiltIn("Not A Real Component"));

    registry.Unregister(s_GaugeTypeName);
    CHECK(!registry.IsBuiltIn(s_GaugeTypeName));
}

TEST(ComponentFactory, TypeNames_lists_built_ins_and_registrations_in_a_stable_order)
{
    CComponentFactoryRegistry registry;
    const std::vector<std::string> builtIns = registry.TypeNames();
    CHECK(builtIns.size() == 7);
    CHECK(builtIns.front() == "3D Text");
    CHECK(builtIns.back() == "Transform");

    registry.Register(s_GaugeTypeName, []()
    {
        return std::make_shared<CTestGaugeComponent>();
    });
    const std::vector<std::string> withGauge = registry.TypeNames();
    CHECK(withGauge.size() == builtIns.size() + 1);
    CHECK(std::is_sorted(withGauge.begin(), withGauge.end()));

    registry.Unregister(s_GaugeTypeName);
    CHECK(registry.TypeNames() == builtIns);
}

TEST(ComponentFactory, separate_registries_do_not_share_registrations)
{
    CComponentFactoryRegistry hostRegistry;
    CComponentFactoryRegistry pluginRegistry;

    CHECK(hostRegistry.Register(s_GaugeTypeName, []()
    {
        return std::make_shared<CTestGaugeComponent>();
    }));

    CHECK(pluginRegistry.Create(s_GaugeTypeName) == nullptr);
    CHECK(hostRegistry.Create(s_GaugeTypeName) != nullptr);
    CHECK(pluginRegistry.Create("Transform") != nullptr);
}
