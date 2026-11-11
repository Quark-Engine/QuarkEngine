#include "engine/material_texture_restore.h"
#include "QuarkCore/QuarkCore.hpp"

namespace quark
{

    SMaterialTextureRestore PlanMaterialTextureRestore(const CMaterialComponent& material,
                                                       const std::vector<STextureOption>& vLibraryTextures)
    {
        SMaterialTextureRestore restore;

        if (!material.m_AlbedoTextureName.empty())
        {
            for (const auto& option : vLibraryTextures)
            {
                if (option.m_Name == material.m_AlbedoTextureName)
                {
                    restore.m_Action = EMaterialTextureRestore::DIRECT_TEXTURE;
                    restore.m_DirectTexture = option.m_Texture;
                    return restore;
                }
            }

            return restore;
        }

        if (!material.m_TextureName.empty())
        {
            restore.m_Action = EMaterialTextureRestore::MATERIAL_FILE;
            return restore;
        }

        if (material.m_TextureSource == TEXTURE_MODEL)
        {
            restore.m_Action = EMaterialTextureRestore::MODEL_TEXTURES;
        }
        else if (material.m_TextureSource == TEXTURE_NONE)
        {
            restore.m_Action = EMaterialTextureRestore::CLEAR_TEXTURES;
        }

        return restore;
    }

    void RestoreOriginalMaterialTextures(CMeshComponent& mesh, const CMaterialComponent& material)
    {
        if (!mesh.m_Model.materials ||
            material.m_vOriginalMaterialTextures.size() != static_cast<size_t>(mesh.m_Model.materialCount))
        {
            return;
        }

        for (int index = 0; index < mesh.m_Model.materialCount; ++index)
        {
            if (mesh.m_Model.materials[index].maps)
            {
                mesh.m_Model.materials[index].maps[MATERIAL_MAP_ALBEDO].texture =
                    material.m_vOriginalMaterialTextures[index];
            }
        }
    }

} // quark
