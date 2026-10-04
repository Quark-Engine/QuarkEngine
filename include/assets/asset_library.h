#ifndef __ASSETS_ASSET_LIBRARY_H__
#define __ASSETS_ASSET_LIBRARY_H__
#include "../entity.h"
#include "../tex.h"
#include "../text_mesh.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

class CScene;
class CTaskPool;

struct SScannedTexture
{
    std::string name;
    std::string fingerprint;
    qc::Image imageData{};
    STextureMeta meta;
    bool hasImage = false;
    std::string error;
};

struct SResourceScanResult
{
    std::string signature;
    std::vector<std::filesystem::path> vModelPaths;
    std::vector<SScannedTexture> vTextures;
};

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
    void SetTaskPool(CTaskPool& taskPool);

    void Refresh(const std::string& projectPath, CScene* pScene);
    void RequestRefresh(const std::string& projectPath);
    void Refresh(const std::string& projectPath, CScene& scene)
    {
        Refresh(projectPath, &scene);
    }

    bool PollResources(const std::string& projectPath, CScene& scene, double now);

    void Unload();

    const std::vector<CModelAsset>& Models() const
    {
        return m_vModels;
    }
    std::vector<CModelAsset>& Models()
    {
        return m_vModels;
    }
    size_t ModelCount() const
    {
        return m_vModels.size();
    }
    CModelAsset* ModelByIndex(size_t index)
    {
        return &m_vModels[index];
    }

    const std::vector<STextureOption>& Textures() const
    {
        return m_vTextures;
    }
    std::vector<STextureOption>& Textures()
    {
        return m_vTextures;
    }
    size_t TextureCount() const
    {
        return m_vTextures.size();
    }
    const STextureOption& TextureByIndex(size_t index) const
    {
        return m_vTextures[index];
    }
    STextureOption& TextureByIndex(size_t index)
    {
        return m_vTextures[index];
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
    void RefreshModels(const std::string& projectPath, CScene& scene,
        const std::vector<std::filesystem::path>& vModelPaths);
    void StartResourceScan(const std::string& projectPath);
    bool ApplyResourceScan(SResourceScanResult& scan, const std::string& projectPath,
        CScene& scene);

    std::vector<CModelAsset> m_vModels;
    std::vector<STextureOption> m_vTextures;
    std::unordered_map<std::string, std::string> m_TextureFingerprints;

    CFreetypeTextMesh* m_pTextMesh = nullptr;
    CTaskPool* m_pTaskPool = nullptr;
    std::future<SResourceScanResult> m_ResourceScanFuture;
    std::string m_ResourceScanPath;
    std::string m_RequestedRefreshPath;
    std::uint64_t m_ResourceScanGeneration = 0;
    std::uint64_t m_ActiveScanGeneration = 0;
    bool m_ForceResourceScan = false;
    bool m_ResourceScanFailed = false;

    double m_LastPollTime = 0.0;
    std::string m_LastResourceSignature;
};

#endif // __ASSETS_ASSET_LIBRARY_H__
