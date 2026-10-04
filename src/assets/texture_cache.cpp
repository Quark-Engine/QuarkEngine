#include "assets/texture_cache.h"

#include <filesystem>

CTextureCache::~CTextureCache() = default;

const qc::Texture2D* CTextureCache::Load(const std::string& imagePath)
{
    const auto it = m_Textures.find(imagePath);
    if (it != m_Textures.end())
    {
        return &it->second;
    }

    if (!std::filesystem::exists(imagePath))
    {
        return nullptr;
    }

    const qc::Texture2D texture = qc::LoadTexture(imagePath.c_str());
    if (texture.id == 0)
    {
        return nullptr;
    }

    return &m_Textures.emplace(imagePath, texture).first->second;
}

const qc::Texture2D* CTextureCache::Find(const std::string& imagePath) const
{
    const auto it = m_Textures.find(imagePath);
    return it == m_Textures.end() ? nullptr : &it->second;
}

bool CTextureCache::Contains(const std::string& imagePath) const
{
    return m_Textures.find(imagePath) != m_Textures.end();
}

void CTextureCache::Unload()
{
    for (auto& pair : m_Textures)
    {
        if (pair.second.id != 0)
        {
            qc::UnloadTexture(pair.second);
        }
    }
    m_Textures.clear();
}
