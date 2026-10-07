#ifndef __TEX_H__
#define __TEX_H__
#include "editor/editor_preferences.h"
#include "entity.h"
#include "scene.h"

#include <filesystem>
#include <string>

struct STextureOption
{
    std::string Name;
    Texture2D Texture;
};

struct STextureMeta
{
    std::string Guid;
    int MipMapMode = 0;
    bool EnableMipMap = false;
    bool SrgbTexture = true;
    bool IsReadable = false;
    int FilterMode = 0;
    int WrapU = 1;
    int WrapV = 1;
    int MaxTextureSize = 2048;
    int CompressionQuality = 50;
    int SpriteMode = 1;
    int TextureType = 8;
    bool AlphaIsTransparency = true;
};

class CTextureMetadataStore
{
public:
    static bool IsImageFile(const std::filesystem::path& path);
    static bool Ensure(const std::filesystem::path& texturePath);
    static bool Load(const std::filesystem::path& texturePath, STextureMeta& meta);
    static bool Save(const std::filesystem::path& texturePath, const STextureMeta& meta);
    static std::filesystem::path PathFromMeta(const std::filesystem::path& path);
    static void ApplyToTexture(Texture2D& texture, const STextureMeta& meta);
};

class CEntityTextureService
{
public:
    static void ApplyTextureRepeat(CEntity& entity);
    static void StoreUV(CEntity* pEntity);
    static void MarkEntityUVDirty(CEntity* pEntity);
    static void MarkEntityBoundsDirty(CEntity* pEntity);
    static void RefreshEntityRenderState(CEntity& entity);
    static void StoreMaterialTextures(CEntity* pEntity);
    static void RestoreModelTextures(CEntity* pEntity);
    static void ClearMaterialTextures(CEntity* pEntity);
    static void DrawEntityWithTexture(CEntity& entity, const Mat4& worldTransform, const CPreferences& preferences);
    static void CloneModelMaterials(CEntity* pEntity);
};

#endif // __TEX_H__
