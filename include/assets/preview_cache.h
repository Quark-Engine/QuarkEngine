#ifndef __ASSETS_PREVIEW_CACHE_H__
#define __ASSETS_PREVIEW_CACHE_H__
#include "QuarkCore/QuarkCore.hpp"

#include <string>
#include <unordered_map>

class CPreviewCache
{
public:
    CPreviewCache() = default;
    ~CPreviewCache();

    CPreviewCache(const CPreviewCache&) = delete;
    CPreviewCache& operator=(const CPreviewCache&) = delete;

    void Unload();

    bool HasModelPreview(const std::string& cacheKey) const;
    qc::Texture ModelPreview(const std::string& cacheKey) const;
    void StoreModelPreview(const std::string& cacheKey, qc::RenderTexture2D renderTexture);

    void InvalidateModelPreviews();

    bool HasMaterialPreview(const std::string& materialPath) const;
    qc::Texture MaterialPreview(const std::string& materialPath) const;
    void StoreMaterialPreview(const std::string& materialPath, qc::RenderTexture2D renderTexture);

    void InvalidateMaterialPreviews();

    void EnsureIcons();

    const qc::Texture& IconFile() const
    {
        return m_IconFile;
    }
    const qc::Texture& IconFolder() const
    {
        return m_IconFolder;
    }
    const qc::Texture& IconFullFolder() const
    {
        return m_IconFullFolder;
    }

private:
    static void UnloadAll(std::unordered_map<std::string, qc::RenderTexture2D>& previews);

    std::unordered_map<std::string, qc::RenderTexture2D> m_ModelPreviews;
    std::unordered_map<std::string, qc::RenderTexture2D> m_MaterialPreviews;

    qc::Texture m_IconFile = { 0 };
    qc::Texture m_IconFolder = { 0 };
    qc::Texture m_IconFullFolder = { 0 };
};

#endif // __ASSETS_PREVIEW_CACHE_H__
