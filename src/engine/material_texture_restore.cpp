#include "engine/material_texture_restore.h"

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
            if (option.Name == material.m_AlbedoTextureName)
            {
                restore.Action = EMaterialTextureRestore::DirectTexture;
                restore.DirectTexture = option.Texture;
                return restore;
            }
        }

        return restore;
    }

    if (!material.m_TextureName.empty())
    {
        restore.Action = EMaterialTextureRestore::MaterialFile;
        return restore;
    }

    if (material.m_TextureSource == TEXTURE_MODEL)
    {
        restore.Action = EMaterialTextureRestore::ModelTextures;
    }
    else if (material.m_TextureSource == TEXTURE_NONE)
    {
        restore.Action = EMaterialTextureRestore::ClearTextures;
    }

    return restore;
}

} // quark