#include "assets/texture_cache.h"

#include <cstdint>
#include <filesystem>

CTextureCache::~CTextureCache() = default;

const qc::Texture2D* CTextureCache::Load(const std::string& imagePath)
{
    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::is_regular_file(imagePath, ec) || ec)
    {
        const auto existing = m_Textures.find(imagePath);
        if (existing != m_Textures.end())
        {
            if (existing->second.id != 0)
            {
                qc::UnloadTexture(existing->second);
            }
            m_Textures.erase(existing);
            m_Fingerprints.erase(imagePath);
        }
        return nullptr;
    }

    const uintmax_t fileSize = fs::file_size(imagePath, ec);
    if (ec)
    {
        return nullptr;
    }
    ec.clear();
    const auto writeTime = fs::last_write_time(imagePath, ec);
    if (ec)
    {
        return nullptr;
    }

    const std::string fingerprint = std::to_string(fileSize) + "|" +
        std::to_string(static_cast<long long>(writeTime.time_since_epoch().count()));
    auto existing = m_Textures.find(imagePath);
    if (existing != m_Textures.end())
    {
        const auto fingerprintIt = m_Fingerprints.find(imagePath);
        if (fingerprintIt != m_Fingerprints.end() && fingerprintIt->second == fingerprint)
        {
            return &existing->second;
        }
    }

    const qc::Texture2D texture = qc::LoadTexture(imagePath.c_str());
    if (texture.id == 0)
    {
        qc::TraceLog(qc::LogLevel::Error, "ASSETS",
            qc::TextFormat("Failed to load texture file: %s", imagePath.c_str()));
        return existing != m_Textures.end() ? &existing->second : nullptr;
    }

    if (existing != m_Textures.end())
    {
        if (existing->second.id != 0)
        {
            qc::UnloadTexture(existing->second);
        }
        existing->second = texture;
        m_Fingerprints[imagePath] = fingerprint;
        return &existing->second;
    }

    auto inserted = m_Textures.emplace(imagePath, texture);
    m_Fingerprints.emplace(imagePath, fingerprint);
    return &inserted.first->second;
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
    m_Fingerprints.clear();
}
