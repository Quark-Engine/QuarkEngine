#ifndef __ENGINE_MATERIAL_TEXTURE_RESTORE_H__
#define __ENGINE_MATERIAL_TEXTURE_RESTORE_H__
#include "../component.h"
#include "../tex.h"

#include <vector>

namespace quark
{

enum class EMaterialTextureRestore
{
    None,
    DirectTexture,
    MaterialFile,
    ModelTextures,
    ClearTextures
};

struct SMaterialTextureRestore
{
    EMaterialTextureRestore Action = EMaterialTextureRestore::None;
    Texture2D DirectTexture = {0};
};

SMaterialTextureRestore PlanMaterialTextureRestore(const CMaterialComponent& material,
    const std::vector<STextureOption>& vLibraryTextures);

void RestoreOriginalMaterialTextures(CMeshComponent& mesh, const CMaterialComponent& material);

} // quark

#endif // __ENGINE_MATERIAL_TEXTURE_RESTORE_H__