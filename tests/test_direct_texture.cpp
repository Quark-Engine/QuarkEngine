#include "test_harness.h"

#include "engine/material_texture_restore.h"
#include "engine/scene_document.h"

#include "QuarkCore/QuarkCore.hpp"

#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace
{

Texture2D FakeTexture(unsigned int id)
{
    Texture2D texture = {};
    texture.id = id;
    texture.width = 4;
    texture.height = 4;
    texture.mipmaps = 1;
    return texture;
}

std::vector<STextureOption> MakeTextureLibrary()
{
    std::vector<STextureOption> vLibrary;
    vLibrary.push_back({ "None", {0} });
    vLibrary.push_back({ "brick.png", FakeTexture(11) });
    vLibrary.push_back({ "textures/rust.png", FakeTexture(22) });
    return vLibrary;
}

int AddEntity(CScene& target, const std::string& name, int parentId = -1)
{
    CEntity added(static_cast<int>(target.m_vEntities.size()));
    added.m_Name = name;
    added.m_ParentId = parentId;
    target.m_vEntities.push_back(added);
    return static_cast<int>(target.m_vEntities.size()) - 1;
}

CMaterialComponent* MaterialOf(CScene& scene, int index)
{
    return scene.m_vEntities[index].GetMaterialComponent();
}

CMaterialComponent* EnsureMaterial(CScene& scene, int index)
{
    if (CMaterialComponent* pExisting = MaterialOf(scene, index))
    {
        return pExisting;
    }
    CComponentManager* pComponents = scene.m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return nullptr;
    }
    auto pMaterial = std::make_shared<CMaterialComponent>();
    pComponents->AddComponent(pMaterial);
    return pMaterial.get();
}

void AssignDirectTexture(CScene& scene, int index, const std::string& textureName,
    unsigned int textureId = 11)
{
    CMaterialComponent* pMaterial = EnsureMaterial(scene, index);
    if (!pMaterial)
    {
        return;
    }
    pMaterial->m_AlbedoTextureName = textureName;
    pMaterial->m_TextureName.clear();
    pMaterial->m_Texture = FakeTexture(textureId);
    pMaterial->m_TextureSource = TEXTURE_EXTERNAL;
}

class SDirectTextureHistory
{
public:
    SDirectTextureHistory(CScene& scene, const CComponentFactoryRegistry& registry)
        : m_Scene(scene)
        , m_Registry(registry)
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

} // anonymous

TEST(DirectTexture, assignment_survives_a_snapshot_round_trip)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "crate");
    AssignDirectTexture(source, index, "brick.png");

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));

    CHECK_MSG(restored.m_vEntities.size() == 1, "the textured entity was dropped by the round-trip");
    if (restored.m_vEntities.empty())
    {
        return;
    }

    const CMaterialComponent* pMaterial = restored.m_vEntities[0].GetMaterialComponent();
    CHECK(pMaterial != nullptr);
    if (!pMaterial)
    {
        return;
    }
    CHECK(pMaterial->m_AlbedoTextureName == "brick.png");
    CHECK(pMaterial->m_TextureName.empty());
    CHECK(pMaterial->m_TextureSource == TEXTURE_EXTERNAL);
}

TEST(DirectTexture, direct_texture_survives_undoing_two_created_objects)
{
    CComponentFactoryRegistry registry;
    CScene scene;
    SDirectTextureHistory history(scene, registry);

    history.Save();
    AddEntity(scene, "crate");
    AssignDirectTexture(scene, 0, "grass.jpg");
    history.Save();
    AddEntity(scene, "lamp");
    history.Save();
    AddEntity(scene, "tree");
    CHECK(CountEntities(scene) == 3);

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 2);

    CHECK(history.Undo());
    CHECK(CountEntities(scene) == 1);

    CHECK_MSG(!scene.m_vEntities.empty(), "undo removed every entity instead of the created one");
    if (scene.m_vEntities.empty())
    {
        return;
    }

    const CMaterialComponent* pMaterial = scene.m_vEntities[0].GetMaterialComponent();
    CHECK(pMaterial != nullptr);
    if (!pMaterial)
    {
        return;
    }
    CHECK_MSG(pMaterial->m_AlbedoTextureName == "grass.jpg",
        "undo of an unrelated action dropped the direct texture");
    CHECK(pMaterial->m_TextureName.empty());
    CHECK(pMaterial->m_TextureSource == TEXTURE_EXTERNAL);
}

TEST(DirectTexture, direct_texture_survives_repeated_undo_redo_cycles)
{
    CComponentFactoryRegistry registry;
    CScene scene;
    SDirectTextureHistory history(scene, registry);

    history.Save();
    AddEntity(scene, "crate");
    AssignDirectTexture(scene, 0, "textures/rust.png", 22);
    history.Save();
    AddEntity(scene, "lamp");

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        CHECK(history.Undo());
        CHECK(CountEntities(scene) == 1);
        CHECK(MaterialOf(scene, 0) != nullptr);
        if (!MaterialOf(scene, 0))
        {
            return;
        }
        CHECK(MaterialOf(scene, 0)->m_AlbedoTextureName == "textures/rust.png");
        CHECK(MaterialOf(scene, 0)->m_TextureSource == TEXTURE_EXTERNAL);

        CHECK(history.Redo());
        CHECK(CountEntities(scene) == 2);
        CHECK(MaterialOf(scene, 0) != nullptr);
        if (!MaterialOf(scene, 0))
        {
            return;
        }
        CHECK(MaterialOf(scene, 0)->m_AlbedoTextureName == "textures/rust.png");
        CHECK(MaterialOf(scene, 0)->m_TextureSource == TEXTURE_EXTERNAL);
    }
}

TEST(DirectTexture, undo_restores_the_name_so_the_runtime_can_rebind_the_handle)
{
    CComponentFactoryRegistry registry;
    CScene scene;
    SDirectTextureHistory history(scene, registry);

    history.Save();
    AddEntity(scene, "crate");
    AssignDirectTexture(scene, 0, "brick.png");
    history.Save();
    AddEntity(scene, "lamp");

    CHECK(history.Undo());

    CMaterialComponent* pMaterial = MaterialOf(scene, 0);
    CHECK(pMaterial != nullptr);
    if (!pMaterial)
    {
        return;
    }
    CHECK_MSG(pMaterial->m_Texture.id == 0, "a gpu handle leaked into the serialized snapshot");

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore =
        quark::PlanMaterialTextureRestore(*pMaterial, vLibrary);
    CHECK(restore.Action == quark::EMaterialTextureRestore::DirectTexture);
    CHECK(restore.DirectTexture.id == 11);
}

TEST(DirectTexture, plan_rebinds_the_direct_texture_from_the_asset_library)
{
    CComponentFactoryRegistry registry;
    CScene source;
    const int index = AddEntity(source, "crate");
    AssignDirectTexture(source, index, "textures/rust.png", 22);

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(quark::CSceneDocument::Serialize(source), restored, registry));
    const CMaterialComponent* pMaterial = restored.m_vEntities[0].GetMaterialComponent();
    CHECK(pMaterial != nullptr);
    if (!pMaterial)
    {
        return;
    }

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore =
        quark::PlanMaterialTextureRestore(*pMaterial, vLibrary);
    CHECK(restore.Action == quark::EMaterialTextureRestore::DirectTexture);
    CHECK(restore.DirectTexture.id == 22);
}

TEST(DirectTexture, plan_never_replaces_a_direct_texture_with_a_material_file)
{
    CMaterialComponent material;
    material.m_AlbedoTextureName = "brick.png";
    material.m_TextureName = "materials/crate.mtl";
    material.m_TextureSource = TEXTURE_EXTERNAL;

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK_MSG(restore.Action == quark::EMaterialTextureRestore::DirectTexture,
        "the material file overrode the direct texture");
    CHECK(restore.DirectTexture.id == 11);
}

TEST(DirectTexture, plan_keeps_an_unresolved_direct_texture_instead_of_loading_another_source)
{
    CMaterialComponent material;
    material.m_AlbedoTextureName = "deleted.png";
    material.m_TextureName = "materials/crate.mtl";
    material.m_TextureSource = TEXTURE_EXTERNAL;

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK_MSG(restore.Action == quark::EMaterialTextureRestore::None,
        "an unresolved direct texture fell back to another texture source");
    CHECK(material.m_AlbedoTextureName == "deleted.png");
}

TEST(DirectTexture, plan_falls_back_to_the_material_file_without_a_direct_texture)
{
    CMaterialComponent material;
    material.m_TextureName = "materials/crate.mtl";
    material.m_TextureSource = TEXTURE_EXTERNAL;

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK(restore.Action == quark::EMaterialTextureRestore::MaterialFile);
}

TEST(DirectTexture, restores_asset_owned_texture_before_unloading_the_model)
{
    CMeshComponent mesh;
    Material modelMaterial = {};
    MaterialMap aMaps[MATERIAL_MAP_BRDF + 1] = {};
    modelMaterial.maps = aMaps;
    mesh.m_Model.materials = &modelMaterial;
    mesh.m_Model.materialCount = 1;

    CMaterialComponent material;
    material.m_vOriginalMaterialTextures.push_back(FakeTexture(22));
    aMaps[MATERIAL_MAP_ALBEDO].texture = FakeTexture(11);

    quark::RestoreOriginalMaterialTextures(mesh, material);

    CHECK(aMaps[MATERIAL_MAP_ALBEDO].texture.id == 22);
}

TEST(DirectTexture, clears_runtime_shadow_maps_before_model_release)
{
    CMeshComponent mesh;
    Material modelMaterial = {};
    MaterialMap aMaps[MATERIAL_MAP_BRDF + 1] = {};
    modelMaterial.maps = aMaps;
    mesh.m_Model.materials = &modelMaterial;
    mesh.m_Model.materialCount = 1;

    for (int shadowIndex = 0; shadowIndex < QC_MAX_LIGHTS; ++shadowIndex)
    {
        aMaps[MATERIAL_MAP_HEIGHT + shadowIndex].texture = FakeTexture(100 + shadowIndex);
    }

    mesh.ClearRuntimeShadowMapBindings();

    for (int shadowIndex = 0; shadowIndex < QC_MAX_LIGHTS; ++shadowIndex)
    {
        const Texture2D& texture = aMaps[MATERIAL_MAP_HEIGHT + shadowIndex].texture;
        CHECK(texture.id == 0);
        CHECK(!texture.valid);
    }
}

TEST(DirectTexture, plan_restores_the_model_textures_for_a_model_sourced_material)
{
    CMaterialComponent material;
    material.m_TextureSource = TEXTURE_MODEL;

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK(restore.Action == quark::EMaterialTextureRestore::ModelTextures);
}

TEST(DirectTexture, plan_clears_textures_when_the_material_has_no_source)
{
    CMaterialComponent material;
    material.m_TextureSource = TEXTURE_NONE;

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK(restore.Action == quark::EMaterialTextureRestore::ClearTextures);
}

TEST(DirectTexture, plan_ignores_the_none_placeholder_of_the_asset_library)
{
    CMaterialComponent material;
    material.m_AlbedoTextureName = "None";

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK_MSG(restore.Action == quark::EMaterialTextureRestore::DirectTexture,
        "the placeholder entry was skipped, so the texture could not be rebound");
    CHECK_MSG(restore.DirectTexture.id == 0, "the placeholder entry must not bind a gpu handle");
}

TEST(DirectTexture, plan_leaves_other_texture_names_untouched)
{
    CMaterialComponent material;
    material.m_NormalTextureName = "brick_n.png";
    material.m_RoughnessTextureName = "brick_r.png";
    material.m_MetallicTextureName = "brick_m.png";
    material.m_AlbedoTextureName = "brick.png";
    material.m_TextureSource = TEXTURE_EXTERNAL;

    const std::vector<STextureOption> vLibrary = MakeTextureLibrary();
    const quark::SMaterialTextureRestore restore = quark::PlanMaterialTextureRestore(material, vLibrary);
    CHECK(restore.Action == quark::EMaterialTextureRestore::DirectTexture);
    CHECK(material.m_NormalTextureName == "brick_n.png");
    CHECK(material.m_RoughnessTextureName == "brick_r.png");
    CHECK(material.m_MetallicTextureName == "brick_m.png");
}