#include "assets/asset_library.h"

#include "models.h"
#include "scene.h"
#include "tex.h"
#include "text_mesh.h"
#include "engine/cpu_task_pool.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

using namespace qc;

namespace fs = std::filesystem;

namespace
{

std::vector<fs::directory_entry> CollectResourceEntries(const fs::path& resourceDir)
{
    std::vector<fs::directory_entry> vResult;
    std::error_code ec;
    fs::recursive_directory_iterator it(resourceDir, fs::directory_options::skip_permission_denied, ec);
    if (ec)
    {
        return vResult;
    }

    for (const auto& entry : it)
    {
        vResult.push_back(entry);
    }

    std::sort(
        vResult.begin(),
        vResult.end(),
        [](const fs::directory_entry& lhs, const fs::directory_entry& rhs)
        {
            std::error_code lhsEc;
            std::error_code rhsEc;
            const bool lhsIsDir = lhs.is_directory(lhsEc) && !lhsEc;
            const bool rhsIsDir = rhs.is_directory(rhsEc) && !rhsEc;

            if (lhsIsDir != rhsIsDir)
            {
                return lhsIsDir;
            }
            return lhs.path().generic_string() < rhs.path().generic_string();
        }
    );

    return vResult;
}

std::vector<fs::path> CollectResourceFiles(const fs::path& resourceDir)
{
    std::vector<fs::path> vResult;
    for (const auto& entry : CollectResourceEntries(resourceDir))
    {
        std::error_code ec;
        if (!entry.is_regular_file(ec) || ec)
        {
            continue;
        }

        vResult.push_back(entry.path());
    }

    return vResult;
}

std::vector<fs::path> CollectModelPaths(const fs::path& resourceDir)
{
    std::vector<fs::path> vResult;
    std::error_code ec;
    fs::recursive_directory_iterator it(resourceDir, fs::directory_options::skip_permission_denied, ec);
    if (ec)
    {
        return vResult;
    }

    for (const auto& entry : it)
    {
        if (!entry.is_regular_file(ec) || ec)
        {
            ec.clear();
            continue;
        }

        if (CModelService::IsModelFile(entry.path()))
        {
            vResult.push_back(entry.path());
        }
    }

    return vResult;
}

std::string BuildFileFingerprint(const fs::path& path)
{
    std::string fingerprint;
    const auto appendFileState = [&fingerprint](const fs::path& filePath)
    {
        std::error_code stateError;
        if (!fs::is_regular_file(filePath, stateError) || stateError)
        {
            fingerprint += "|missing";
            return;
        }

        stateError.clear();
        const uintmax_t size = fs::file_size(filePath, stateError);
        fingerprint += stateError ? "|size-error" : "|" + std::to_string(size);
        stateError.clear();
        const auto writeTime = fs::last_write_time(filePath, stateError);
        fingerprint += stateError ? "|time-error" : "|" +
            std::to_string(static_cast<long long>(writeTime.time_since_epoch().count()));
    };
    appendFileState(path);
    appendFileState(fs::path(path.string() + ".meta"));
    return fingerprint;
}

std::string BuildResourceSignature(const fs::path& resourceDir)
{
    std::vector<std::string> vEntries;
    std::error_code ec;
    fs::recursive_directory_iterator it(resourceDir, fs::directory_options::skip_permission_denied, ec);
    if (ec)
    {
        return {};
    }

    for (const auto& entry : it)
    {
        std::error_code entryEc;
        const fs::path relative = fs::relative(entry.path(), resourceDir, entryEc);
        if (entryEc)
        {
            continue;
        }

        std::string row = relative.generic_string();
        if (entry.is_regular_file(entryEc) && !entryEc)
        {
            std::error_code sizeEc;
            std::error_code timeEc;
            row += "|f|";
            const uintmax_t size = fs::file_size(entry.path(), sizeEc);
            row += sizeEc ? "0" : std::to_string(size);
            row += "|";
            const auto writeTime = fs::last_write_time(entry.path(), timeEc);
            row += timeEc ? "0" :
                std::to_string(static_cast<long long>(writeTime.time_since_epoch().count()));
        }
        else
        {
            row += "|d";
        }

        vEntries.push_back(std::move(row));
    }

    std::sort(vEntries.begin(), vEntries.end());

    std::string signature;
    for (const auto& entry : vEntries)
    {
        signature += entry;
        signature.push_back('\n');
    }

    return signature;
}

SResourceScanResult ScanResources(const fs::path& resourceDir,
    const std::unordered_map<std::string, std::string>& textureFingerprints)
{
    std::error_code directoryError;
    if (!fs::is_directory(resourceDir, directoryError) || directoryError)
    {
        throw std::runtime_error("Project resources directory is unavailable");
    }

    SResourceScanResult scan;
    const std::vector<fs::directory_entry> vEntries = CollectResourceEntries(resourceDir);

    for (const fs::directory_entry& entry : vEntries)
    {
        std::error_code ec;
        if (!entry.is_regular_file(ec) || ec)
        {
            continue;
        }

        const fs::path path = entry.path();
        if (CModelService::IsModelFile(path))
        {
            scan.vModelPaths.push_back(path);
        }
        if (!CTextureMetadataStore::IsImageFile(path))
        {
            continue;
        }

        SScannedTexture texture;
        texture.name = fs::relative(path, resourceDir, ec).generic_string();
        if (ec)
        {
            texture.name = path.filename().generic_string();
            ec.clear();
        }
        const fs::path metadataPath(path.string() + ".meta");
        if (!fs::exists(metadataPath, ec))
        {
            CTextureMetadataStore::Ensure(path);
        }
        texture.fingerprint = BuildFileFingerprint(path);

        const auto fingerprintIt = textureFingerprints.find(texture.name);
        if (fingerprintIt != textureFingerprints.end() &&
            fingerprintIt->second == texture.fingerprint)
        {
            scan.vTextures.push_back(std::move(texture));
            continue;
        }

        if (!CTextureMetadataStore::Load(path, texture.meta))
        {
            texture.meta = {};
        }

        texture.imageData = LoadImage(path.string().c_str());
        if (!IsImageValid(texture.imageData))
        {
            texture.error = "Failed to decode image";
        }
        else
        {
            ImageFormat(&texture.imageData, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            texture.hasImage = IsImageValid(texture.imageData) &&
                texture.imageData.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            if (!texture.hasImage)
            {
                texture.error = "Failed to convert image to RGBA8";
            }
        }

        scan.vTextures.push_back(std::move(texture));
    }

    std::sort(scan.vModelPaths.begin(), scan.vModelPaths.end());
    scan.signature = BuildResourceSignature(resourceDir);
    return scan;
}

void UnloadScannedImages(SResourceScanResult& scan)
{
    for (SScannedTexture& texture : scan.vTextures)
    {
        if (texture.imageData.data)
        {
            UnloadImage(texture.imageData);
            texture.imageData = {};
        }
        texture.hasImage = false;
    }
}

struct SScannedImageGuard
{
    SResourceScanResult& scan;

    ~SScannedImageGuard()
    {
        UnloadScannedImages(scan);
    }
};

} // anonymous

std::string CAssetLibrary::AssetNameForPath(const fs::path& projectPath, const fs::path& assetPath)
{
    std::error_code ec;
    const fs::path resourceDir = projectPath / "resources";
    const fs::path relative = fs::relative(assetPath, resourceDir, ec);
    if (!ec)
    {
        return relative.generic_string();
    }
    return assetPath.filename().generic_string();
}

CAssetLibrary::~CAssetLibrary()
{
    if (m_ResourceScanFuture.valid())
    {
        m_ResourceScanFuture.wait();
        try
        {
            SResourceScanResult scan = m_ResourceScanFuture.get();
            UnloadScannedImages(scan);
        }
        catch (const std::exception& exception)
        {
            TraceLog(LogLevel::Error, "ASSETS", TextFormat(
                "Resource scan failed during shutdown: %s", exception.what()));
        }
    }
}

void CAssetLibrary::SetTextMesh(CFreetypeTextMesh& textMesh)
{
    m_pTextMesh = &textMesh;
}

void CAssetLibrary::SetTaskPool(CTaskPool& taskPool)
{
    m_pTaskPool = &taskPool;
}
void CAssetLibrary::RegisterProceduralAssets()
{
    CModelAsset cubeAsset;
    cubeAsset.m_Name = "Cube";
    cubeAsset.m_Type = OBJECT_CUBE;
    cubeAsset.m_IsProcedural = true;
    cubeAsset.pfnGenerator = [](int)
    {
        return LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    };
    m_vModels.push_back(std::move(cubeAsset));

    CModelAsset sphereAsset;
    sphereAsset.m_Name = "Sphere";
    sphereAsset.m_Type = OBJECT_SPHERE;
    sphereAsset.m_IsProcedural = true;
    sphereAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshSphere(1.0f, seg, seg));
    };
    m_vModels.push_back(std::move(sphereAsset));

    CModelAsset coneAsset;
    coneAsset.m_Name = "Cone";
    coneAsset.m_Type = OBJECT_CONE;
    coneAsset.m_IsProcedural = true;
    coneAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, seg));
    };
    m_vModels.push_back(std::move(coneAsset));

    CModelAsset cylinderAsset;
    cylinderAsset.m_Name = "Cylinder";
    cylinderAsset.m_Type = OBJECT_CYLINDER;
    cylinderAsset.m_IsProcedural = true;
    cylinderAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, seg));
    };
    m_vModels.push_back(std::move(cylinderAsset));

    CModelAsset hemisphereAsset;
    hemisphereAsset.m_Name = "HemiSphere";
    hemisphereAsset.m_Type = OBJECT_HEMISPHERE;
    hemisphereAsset.m_IsProcedural = true;
    hemisphereAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshHemiSphere(1.0f, seg, seg));
    };
    m_vModels.push_back(std::move(hemisphereAsset));

    CModelAsset torusAsset;
    torusAsset.m_Name = "Torus";
    torusAsset.m_Type = OBJECT_TORUS;
    torusAsset.m_IsProcedural = true;
    torusAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshTorus(1.0f, 1.0f, seg, seg));
    };
    m_vModels.push_back(std::move(torusAsset));

    CModelAsset textAsset;
    textAsset.m_Name = "3D Text";
    textAsset.m_Type = OBJECT_CUBE;
    textAsset.m_IsProcedural = true;
    textAsset.pfnGenerator = [this](int)
    {
        if (!m_pTextMesh)
        {
            TraceLog(LogLevel::Warn, "ASSETS", "'3D Text' asset needs a CFreetypeTextMesh; generating an empty model");
            return Model{};
        }
        std::string fontPath = CFreetypeTextMesh::GetDefaultFontPath();
        return m_pTextMesh->Generate("Text", 0.3f, 0.15f, 0.2f, fontPath);
    };

    m_vModels.push_back(std::move(textAsset));
}

void CAssetLibrary::Load(const std::string& projectPath)
{
    RegisterProceduralAssets();
    LoadTexturesFromDisk(projectPath);
}

void CAssetLibrary::LoadTexturesFromDisk(const std::string& projectPath)
{
    fs::path resourceDir = fs::path(projectPath) / "resources";
    if (!fs::exists(resourceDir))
    {
        fs::create_directories(resourceDir);
    }

    UnloadTextures();
    m_vTextures.clear();
    m_TextureFingerprints.clear();
    m_vTextures.push_back({ "None", {0} });

    for (const auto& path : CollectResourceFiles(resourceDir))
    {
        if (!CTextureMetadataStore::IsImageFile(path))
        {
            continue;
        }

        CTextureMetadataStore::Ensure(path);
        qc::Texture2D tex = LoadTexture(path.string().c_str());
        STextureMeta meta;
        if (CTextureMetadataStore::Load(path, meta))
        {
            CTextureMetadataStore::ApplyToTexture(tex, meta);
        }
        const std::string name = fs::relative(path, resourceDir).generic_string();
        m_vTextures.push_back({ name, tex });
        m_TextureFingerprints[name] = BuildFileFingerprint(path);
    }
}

void CAssetLibrary::UnloadTextures()
{
    std::unordered_set<unsigned int> releasedIds;
    for (auto& option : m_vTextures)
    {
        if (option.Texture.id == 0)
        {
            continue;
        }
        if (releasedIds.insert(option.Texture.id).second)
        {
            UnloadTexture(option.Texture);
        }
    }
    m_vTextures.clear();
}

void CAssetLibrary::RefreshTextures(const std::string& projectPath, CScene* pScene)
{
    fs::path resourceDir = fs::path(projectPath) / "resources";
    if (!fs::exists(resourceDir))
    {
        fs::create_directories(resourceDir);
    }

    std::unordered_map<std::string, qc::Texture2D> oldByName;
    for (auto& option : m_vTextures)
    {
        if (option.Texture.id != 0)
        {
            oldByName[option.Name] = option.Texture;
        }
    }

    std::vector<STextureOption> vNextOptions;
    std::unordered_map<std::string, std::string> nextFingerprints;
    vNextOptions.push_back({ "None", {0} });

    for (const auto& path : CollectResourceFiles(resourceDir))
    {
        if (!CTextureMetadataStore::IsImageFile(path))
        {
            continue;
        }

        const std::string textureName = fs::relative(path, resourceDir).generic_string();
        CTextureMetadataStore::Ensure(path);
        const std::string fingerprint = BuildFileFingerprint(path);
        auto oldIt = oldByName.find(textureName);
        if (oldIt != oldByName.end())
        {
            vNextOptions.push_back({ textureName, oldIt->second });
            oldByName.erase(oldIt);
            nextFingerprints[textureName] = fingerprint;
            continue;
        }

        qc::Texture2D tex = LoadTexture(path.string().c_str());
        vNextOptions.push_back({ textureName, tex });
        nextFingerprints[textureName] = fingerprint;
    }

    if (pScene)
    {
        for (const auto& [_, removedTex] : oldByName)
        {
            for (auto& entity : pScene->m_vEntities)
            {
                CMeshComponent* pMesh = entity.GetMeshComponent();
                CMaterialComponent* pMat = entity.GetMaterialComponent();
                if (pMesh && pMat && pMat->m_Texture.id == removedTex.id)
                {
                    pMat->m_Texture = {0};
                }
            }
        }
    }

    std::unordered_set<unsigned int> releasedIds;
    for (auto& [_, removedTex] : oldByName)
    {
        if (removedTex.id == 0)
        {
            continue;
        }
        if (releasedIds.insert(removedTex.id).second)
        {
            UnloadTexture(removedTex);
        }
    }

    m_vTextures = std::move(vNextOptions);
    m_TextureFingerprints = std::move(nextFingerprints);
}

void CAssetLibrary::RefreshModels(const std::string& projectPath, CScene& scene)
{
    RefreshModels(projectPath, scene, CollectModelPaths(fs::path(projectPath) / "resources"));
}

void CAssetLibrary::RefreshModels(const std::string& projectPath, CScene& scene,
    const std::vector<fs::path>& vModelPaths)
{
    std::unordered_map<std::string, qc::Model> old;

    for (auto& asset : m_vModels)
    {
        if (!asset.m_IsProcedural)
        {
            qc::Model model = asset.TakeLoadedModel();
            if (model.meshCount > 0 && model.meshes)
            {
                old[asset.m_Name] = model;
            }
            else
            {
                UnloadModel(model);
            }
        }
    }

    std::vector<CModelAsset> vNext;
    vNext.reserve(m_vModels.size());

    for (auto& asset : m_vModels)
    {
        if (asset.m_IsProcedural)
        {
            vNext.push_back(std::move(asset));
        }
    }

    fs::path resourceDir = fs::path(projectPath) / "resources";
    if (!fs::exists(resourceDir))
    {
        fs::create_directories(resourceDir);
    }

    for (const auto& path : vModelPaths)
    {
        std::string name = fs::relative(path, resourceDir).generic_string();

        CModelAsset asset;
        asset.m_Name = name;
        asset.m_Type = OBJECT_CUBE;
        asset.m_IsProcedural = false;
        asset.m_FilePath = path.string();

        if (old.count(name))
        {
            asset.m_LoadedModel = old[name];
            old.erase(name);
        }
        else
        {
            if (!CModelService::EnsureAssetLoaded(asset))
            {
                continue;
            }
        }

        vNext.push_back(std::move(asset));
    }

    for (auto& [_, loadedModel] : old)
    {
        UnloadModel(loadedModel);
    }

    m_vModels = std::move(vNext);

    for (auto& entity : scene.m_vEntities)
    {
        CMeshComponent* pMesh = entity.GetMeshComponent();
        if (!pMesh || pMesh->m_AssetName.empty())
        {
            continue;
        }
        pMesh->m_pAsset = nullptr;
        for (auto& asset : m_vModels)
        {
            if (asset.m_Name == pMesh->m_AssetName)
            {
                pMesh->m_pAsset = &asset;
                break;
            }
        }
    }
}

void CAssetLibrary::Refresh(const std::string& projectPath, CScene* pScene)
{
    ++m_ResourceScanGeneration;
    m_ForceResourceScan = false;
    m_RequestedRefreshPath.clear();
    RefreshTextures(projectPath, pScene);
    if (pScene)
    {
        RefreshModels(projectPath, *pScene);
    }
    m_LastResourceSignature = BuildResourceSignature(fs::path(projectPath) / "resources");
}

void CAssetLibrary::RequestRefresh(const std::string& projectPath)
{
    ++m_ResourceScanGeneration;
    m_RequestedRefreshPath = projectPath;
    m_ForceResourceScan = true;
}

void CAssetLibrary::StartResourceScan(const std::string& projectPath)
{
    if (!m_pTaskPool)
    {
        TraceLog(LogLevel::Error, "ASSETS", "Cannot scan resources asynchronously: CPU task pool is not configured");
        return;
    }

    const fs::path resourceDir = fs::path(projectPath) / "resources";
    const auto textureFingerprints = m_TextureFingerprints;
    m_ResourceScanPath = projectPath;
    m_ActiveScanGeneration = m_ResourceScanGeneration;
    m_ResourceScanFuture = m_pTaskPool->Submit(
        "Scan project assets and decode changed textures", [resourceDir, textureFingerprints]()
    {
        return ScanResources(resourceDir, textureFingerprints);
    });
}

bool CAssetLibrary::ApplyResourceScan(SResourceScanResult& scan,
    const std::string& projectPath, CScene& scene)
{
    SScannedImageGuard imageGuard{ scan };
    if (scan.signature == m_LastResourceSignature)
    {
        return false;
    }

    std::unordered_map<std::string, qc::Texture2D> oldByName;
    for (const STextureOption& option : m_vTextures)
    {
        if (option.Texture.id != 0)
        {
            oldByName[option.Name] = option.Texture;
        }
    }

    std::vector<STextureOption> vNextTextures;
    std::unordered_map<std::string, std::string> nextFingerprints;
    vNextTextures.push_back({ "None", {0} });

    for (SScannedTexture& scannedTexture : scan.vTextures)
    {
        const auto oldIt = oldByName.find(scannedTexture.name);
        if (!scannedTexture.hasImage)
        {
            if (scannedTexture.error.empty() && oldIt != oldByName.end())
            {
                vNextTextures.push_back({ scannedTexture.name, oldIt->second });
                oldByName.erase(oldIt);
                const auto fingerprintIt = m_TextureFingerprints.find(scannedTexture.name);
                if (fingerprintIt != m_TextureFingerprints.end())
                {
                    nextFingerprints[scannedTexture.name] = fingerprintIt->second;
                }
            }
            else if (!scannedTexture.error.empty())
            {
                TraceLog(LogLevel::Error, "ASSETS", TextFormat("%s: %s",
                    scannedTexture.name.c_str(), scannedTexture.error.c_str()));
                if (oldIt != oldByName.end())
                {
                    vNextTextures.push_back({ scannedTexture.name, oldIt->second });
                    oldByName.erase(oldIt);
                    const auto fingerprintIt = m_TextureFingerprints.find(scannedTexture.name);
                    if (fingerprintIt != m_TextureFingerprints.end())
                    {
                        nextFingerprints[scannedTexture.name] = fingerprintIt->second;
                    }
                }
            }
            continue;
        }

        const auto fingerprintIt = m_TextureFingerprints.find(scannedTexture.name);
        if (oldIt != oldByName.end() &&
            fingerprintIt != m_TextureFingerprints.end() &&
            fingerprintIt->second == scannedTexture.fingerprint)
        {
            vNextTextures.push_back({ scannedTexture.name, oldIt->second });
            oldByName.erase(oldIt);
            nextFingerprints[scannedTexture.name] = scannedTexture.fingerprint;
            continue;
        }

        qc::Texture2D texture = LoadTextureFromImage(scannedTexture.imageData);
        if (texture.id == 0)
        {
            TraceLog(LogLevel::Error, "ASSETS", TextFormat(
                "%s: GPU texture creation failed", scannedTexture.name.c_str()));
            if (oldIt != oldByName.end())
            {
                vNextTextures.push_back({ scannedTexture.name, oldIt->second });
                oldByName.erase(oldIt);
                const auto fingerprintIt = m_TextureFingerprints.find(scannedTexture.name);
                if (fingerprintIt != m_TextureFingerprints.end())
                {
                    nextFingerprints[scannedTexture.name] = fingerprintIt->second;
                }
            }
            continue;
        }
        CTextureMetadataStore::ApplyToTexture(texture, scannedTexture.meta);
        vNextTextures.push_back({ scannedTexture.name, texture });
        nextFingerprints[scannedTexture.name] = scannedTexture.fingerprint;
    }
    for (const auto& [oldName, removedTexture] : oldByName)
    {
        auto replacement = std::find_if(vNextTextures.begin(), vNextTextures.end(),
            [&oldName](const STextureOption& option)
            {
                return option.Name == oldName;
            });
        for (CEntity& entity : scene.m_vEntities)
        {
            CMeshComponent* pMesh = entity.GetMeshComponent();
            CMaterialComponent* pMaterial = entity.GetMaterialComponent();
            if (pMesh && pMaterial && pMaterial->m_Texture.id == removedTexture.id)
            {
                pMaterial->m_Texture = replacement != vNextTextures.end()
                    ? replacement->Texture
                    : qc::Texture2D{0};
            }
        }
    }

    std::unordered_set<unsigned int> releasedIds;
    for (const auto& [_, removedTexture] : oldByName)
    {
        if (removedTexture.id != 0 && releasedIds.insert(removedTexture.id).second)
        {
            UnloadTexture(removedTexture);
        }
    }

    m_vTextures = std::move(vNextTextures);
    m_TextureFingerprints = std::move(nextFingerprints);
    RefreshModels(projectPath, scene, scan.vModelPaths);
    m_LastResourceSignature = scan.signature;
    return true;
}

bool CAssetLibrary::PollResources(const std::string& projectPath, CScene& scene, double now)
{
    bool resourcesChanged = false;
    if (m_ResourceScanFuture.valid() &&
        m_ResourceScanFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        SResourceScanResult scan;
        try
        {
            scan = m_ResourceScanFuture.get();
        }
        catch (const std::exception& exception)
        {
            TraceLog(LogLevel::Error, "ASSETS", TextFormat(
                "Background resource scan failed: %s", exception.what()));
            m_ResourceScanFailed = true;
        }

        if (!m_ResourceScanFailed &&
            m_ActiveScanGeneration == m_ResourceScanGeneration &&
            m_ResourceScanPath == projectPath)
        {
            resourcesChanged = ApplyResourceScan(scan, projectPath, scene);
        }
        else
        {
            UnloadScannedImages(scan);
        }
        m_ResourceScanFailed = false;
    }

    if (m_ResourceScanFuture.valid())
    {
        return resourcesChanged;
    }

    const bool requested = m_ForceResourceScan;
    if (!requested && now - m_LastPollTime <= POLL_INTERVAL_SECONDS)
    {
        return resourcesChanged;
    }

    m_LastPollTime = now;
    m_ForceResourceScan = false;
    const std::string scanPath = requested && !m_RequestedRefreshPath.empty()
        ? m_RequestedRefreshPath
        : projectPath;
    m_RequestedRefreshPath.clear();
    try
    {
        StartResourceScan(scanPath);
    }
    catch (const std::exception& exception)
    {
        TraceLog(LogLevel::Error, "ASSETS", TextFormat(
            "Failed to start background resource scan: %s", exception.what()));
    }
    return resourcesChanged;
}

void CAssetLibrary::Unload()
{
    ++m_ResourceScanGeneration;
    if (m_ResourceScanFuture.valid())
    {
        m_ResourceScanFuture.wait();
        try
        {
            SResourceScanResult scan = m_ResourceScanFuture.get();
            UnloadScannedImages(scan);
        }
        catch (const std::exception& exception)
        {
            TraceLog(LogLevel::Error, "ASSETS", TextFormat(
                "Background resource scan failed during unload: %s", exception.what()));
        }
    }
    m_ResourceScanPath.clear();
    m_RequestedRefreshPath.clear();
    m_ForceResourceScan = false;
    m_vModels.clear();
    UnloadTextures();
    m_TextureFingerprints.clear();
}

CModelAsset* CAssetLibrary::FindModelByName(const std::string& assetName)
{
    for (auto& asset : m_vModels)
    {
        if (asset.m_Name == assetName)
        {
            return &asset;
        }
    }
    return nullptr;
}

const CModelAsset* CAssetLibrary::FindModelByName(const std::string& assetName) const
{
    for (const auto& asset : m_vModels)
    {
        if (asset.m_Name == assetName)
        {
            return &asset;
        }
    }
    return nullptr;
}

CModelAsset* CAssetLibrary::FindModelByPath(const fs::path& fullPath, const fs::path& projectPath)
{
    return FindModelByName(AssetNameForPath(projectPath, fullPath));
}

const CModelAsset* CAssetLibrary::FindModelByPath(const fs::path& fullPath, const fs::path& projectPath) const
{
    return FindModelByName(AssetNameForPath(projectPath, fullPath));
}
