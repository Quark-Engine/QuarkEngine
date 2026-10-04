#include "assets/preview_cache.h"

CPreviewCache::~CPreviewCache() = default;

void CPreviewCache::UnloadAll(std::unordered_map<std::string, qc::RenderTexture2D>& previews)
{
    for (auto& pair : previews)
    {
        if (pair.second.id == 0)
        {
            continue;
        }
        qc::UnloadRenderTexture(pair.second);
    }
    previews.clear();
}

void CPreviewCache::Unload()
{
    UnloadAll(m_ModelPreviews);
    UnloadAll(m_MaterialPreviews);

    if (m_IconFile.id != 0)
    {
        qc::UnloadTexture(m_IconFile);
    }
    if (m_IconFolder.id != 0)
    {
        qc::UnloadTexture(m_IconFolder);
    }
    if (m_IconFullFolder.id != 0)
    {
        qc::UnloadTexture(m_IconFullFolder);
    }

    m_IconFile = { 0 };
    m_IconFolder = { 0 };
    m_IconFullFolder = { 0 };
}

bool CPreviewCache::HasModelPreview(const std::string& cacheKey) const
{
    const auto it = m_ModelPreviews.find(cacheKey);
    return it != m_ModelPreviews.end() && it->second.id != 0;
}

qc::Texture CPreviewCache::ModelPreview(const std::string& cacheKey) const
{
    const auto it = m_ModelPreviews.find(cacheKey);
    if (it == m_ModelPreviews.end())
    {
        return { 0 };
    }
    return it->second.texture;
}

void CPreviewCache::StoreModelPreview(const std::string& cacheKey, qc::RenderTexture2D renderTexture)
{
    if (renderTexture.id == 0)
    {
        return;
    }
    m_ModelPreviews[cacheKey] = renderTexture;
}

void CPreviewCache::InvalidateModelPreviews()
{
    UnloadAll(m_ModelPreviews);
}

bool CPreviewCache::HasMaterialPreview(const std::string& materialPath) const
{
    const auto it = m_MaterialPreviews.find(materialPath);
    return it != m_MaterialPreviews.end() && it->second.id != 0;
}

qc::Texture CPreviewCache::MaterialPreview(const std::string& materialPath) const
{
    const auto it = m_MaterialPreviews.find(materialPath);
    if (it == m_MaterialPreviews.end())
    {
        return { 0 };
    }
    return it->second.texture;
}

void CPreviewCache::StoreMaterialPreview(const std::string& materialPath, qc::RenderTexture2D renderTexture)
{
    if (renderTexture.id == 0)
    {
        return;
    }
    m_MaterialPreviews[materialPath] = renderTexture;
}

void CPreviewCache::InvalidateMaterialPreviews()
{
    UnloadAll(m_MaterialPreviews);
}

void CPreviewCache::EnsureIcons()
{
    if (m_IconFile.id == 0)
    {
        m_IconFile = qc::LoadTexture("assets/file.png");
    }
    if (m_IconFolder.id == 0)
    {
        m_IconFolder = qc::LoadTexture("assets/folder.png");
    }
    if (m_IconFullFolder.id == 0)
    {
        m_IconFullFolder = qc::LoadTexture("assets/full_folder.png");
    }
}
