#ifndef __ASSETS_ASSET_LIBRARY_H__
#define __ASSETS_ASSET_LIBRARY_H__
#include "../entity.h"
#include "../tex.h"
#include "../text_mesh.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class CScene;

class CAssetLibrary
{
public:
    CAssetLibrary() = default;
    ~CAssetLibrary();

    CAssetLibrary(const CAssetLibrary&) = delete;
    CAssetLibrary& operator=(const CAssetLibrary&) = delete;

    void Load(const std::string& projectPath);

    static std::string AssetNameForPath(const std::filesystem::path& projectPath,
                                        const std::filesystem::path& assetPath);

    void SetTextMesh(CFreetypeTextMesh& textMesh);

    void Refresh(const std::string& projectPath, CScene* pScene);
    void Refresh(const std::string& projectPath, CScene& scene)
    {
        Refresh(projectPath, &scene);
    }

    bool PollResources(const std::string& projectPath, CScene& scene, double now);

    void Unload();

    const std::vector<CModelAsset>& Models() const
    {
        return m_Models;
    }
    std::vector<CModelAsset>& Models()
    {
        return m_Models;
    }
    size_t ModelCount() const
    {
        return m_Models.size();
    }
    CModelAsset* ModelByIndex(size_t index)
    {
        return &m_Models[index];
    }

    const std::vector<STextureOption>& Textures() const
    {
        return m_Textures;
    }
    std::vector<STextureOption>& Textures()
    {
        return m_Textures;
    }
    size_t TextureCount() const
    {
        return m_Textures.size();
    }
    const STextureOption& TextureByIndex(size_t index) const
    {
        return m_Textures[index];
    }
    STextureOption& TextureByIndex(size_t index)
    {
        return m_Textures[index];
    }

    CModelAsset* FindModelByName(const std::string& assetName);
    const CModelAsset* FindModelByName(const std::string& assetName) const;
    CModelAsset* FindModelByPath(const std::filesystem::path& fullPath,
                                 const std::filesystem::path& projectPath);
    const CModelAsset* FindModelByPath(const std::filesystem::path& fullPath,
                                       const std::filesystem::path& projectPath) const;

private:
    static constexpr double POLL_INTERVAL_SECONDS = 2.0;

    void RegisterProceduralAssets();
    void LoadTexturesFromDisk(const std::string& projectPath);
    void UnloadTextures();
    void RefreshTextures(const std::string& projectPath, CScene* pScene);
    void RefreshModels(const std::string& projectPath, CScene& scene);

    std::vector<CModelAsset> m_Models;
    std::vector<STextureOption> m_Textures;

    CFreetypeTextMesh* m_pTextMesh = nullptr;

    double m_LastPollTime = 0.0;
    std::string m_LastResourceSignature;
};

#endif // __ASSETS_ASSET_LIBRARY_H__
