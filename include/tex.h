#ifndef __TEX_H__
#define __TEX_H__
#include "editor/editor_preferences.h"
#include "entity.h"
#include "scene.h"

#include <filesystem>
#include <string>

struct STextureOption
{
    std::string m_Name;
    Texture2D m_Texture;
};

struct STextureMeta
{
    std::string m_Guid;
    int m_MipMapMode = 0;
    bool m_EnableMipMap = false;
    bool m_SrgbTexture = true;
    bool m_IsReadable = false;
    int m_FilterMode = 0;
    int m_WrapU = 1;
    int m_WrapV = 1;
    int m_MaxTextureSize = 2048;
    int m_CompressionQuality = 50;
    int m_SpriteMode = 1;
    int m_TextureType = 8;
    bool m_AlphaIsTransparency = true;
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
