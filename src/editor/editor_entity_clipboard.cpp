#include "editor/editor_entity_clipboard.h"

#include "editor/editor.h"
#include "application_plugin_bridge.h"
#include "editor/editor_entity.h"
#include "models.h"

#include <memory>

void CEntityClipboard::Copy(CEditor& editor, CEntity* pEntity)
{
    editor.m_Ui.m_MeshEdit.ClipboardData = *pEntity;
    editor.m_Ui.m_MeshEdit.HasClipboard = true;
}

void CEntityClipboard::Paste(CEditor& editor)
{
    const CMeshComponent* pClipboardMesh = editor.m_Ui.m_MeshEdit.ClipboardData.GetMeshComponent();
    const CTransformComponent* pClipboardTransform = editor.m_Ui.m_MeshEdit.ClipboardData.GetTransformComponent();
    const CLightComponent* pClipboardLight = editor.m_Ui.m_MeshEdit.ClipboardData.GetLightComponent();
    const CMaterialComponent* pClipboardMat = editor.m_Ui.m_MeshEdit.ClipboardData.GetMaterialComponent();
    CModelAsset* pAsset = (pClipboardMesh && !pClipboardMesh->m_AssetName.empty())
        ? editor.m_Assets.FindModelByName(pClipboardMesh->m_AssetName)
        : nullptr;

    if (pAsset)
    {
        editor.SaveState();
        CEntity pasted = CEntityFactory::FromAsset(editor.m_Scene, *pAsset);
        if (auto pPastedTransform = pasted.GetTransformComponent(); pPastedTransform && pClipboardTransform)
        {
            pPastedTransform->m_Position = pClipboardTransform->m_Position;
            pPastedTransform->m_Rotation = pClipboardTransform->m_Rotation;
            pPastedTransform->m_Scale = pClipboardTransform->m_Scale;
        }

        auto pPastedMesh = pasted.GetMeshComponent();
        auto pPastedMat = pasted.GetMaterialComponent();

        if (pPastedMesh && pPastedMat && pClipboardMesh && pClipboardMat)
        {
            pPastedMat->m_Color = pClipboardMat->m_Color;
            pPastedMat->m_OutlineColor = pClipboardMat->m_OutlineColor;
            pPastedMat->m_TextureSource = pClipboardMat->m_TextureSource;
            pPastedMat->m_TextureName = pClipboardMat->m_TextureName;
            pPastedMat->m_Texture = pClipboardMat->m_Texture;
            pPastedMat->m_TextureStretch = pClipboardMat->m_TextureStretch;
            pPastedMat->m_AutoUv = pClipboardMat->m_AutoUv;
            pPastedMat->m_TextureRepeatU = pClipboardMat->m_TextureRepeatU;
            pPastedMat->m_TextureRepeatV = pClipboardMat->m_TextureRepeatV;
            pPastedMat->m_UvScale = pClipboardMat->m_UvScale;
            pPastedMesh->m_MeshTrianglesDetached = pClipboardMesh->m_MeshTrianglesDetached;
            pPastedMesh->m_vMeshVertexOverrides = pClipboardMesh->m_vMeshVertexOverrides;
            CMeshOverrideService::Apply(pasted);
        }
        if (pClipboardLight)
        {
            auto pLightCopy = std::make_shared<CLightComponent>(*pClipboardLight);
            const int lightType = pLightCopy->m_Light.m_Light.type;
            pLightCopy->m_Created = false;
            pLightCopy->m_Light.m_Id = -1;
            pLightCopy->m_Light.m_Light = {};
            pLightCopy->m_Light.m_Light.type = lightType;
            pasted.GetComponents()->AddComponent(pLightCopy);
        }
        editor.m_Scene.m_vEntities.push_back(std::move(pasted));
        const int entityIndex = static_cast<int>(editor.m_Scene.m_vEntities.size()) - 1;
        editor.m_Scene.m_Selected = entityIndex;
        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
    }
}
