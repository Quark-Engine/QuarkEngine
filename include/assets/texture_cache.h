#ifndef __ASSETS_TEXTURE_CACHE_H__
#define __ASSETS_TEXTURE_CACHE_H__
#include "QuarkCore/QuarkCore.hpp"

#include <string>
#include <unordered_map>

class CTextureCache
{
public:
    CTextureCache() = default;
    ~CTextureCache();

    CTextureCache(const CTextureCache&) = delete;
    CTextureCache& operator=(const CTextureCache&) = delete;

    const Texture2D* Load(const std::string& imagePath);

    const Texture2D* Find(const std::string& imagePath) const;

    bool Contains(const std::string& imagePath) const;

    void Unload();

private:
    std::unordered_map<std::string, Texture2D> m_Textures;
    std::unordered_map<std::string, std::string> m_Fingerprints;
};

#endif // __ASSETS_TEXTURE_CACHE_H__
