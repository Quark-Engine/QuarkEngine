#include "assets/asset_library.h"

#include "models.h"
#include "scene.h"
#include "tex.h"
#include "text_mesh.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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
            row += sizeEc ? "0" : std::to_string(fs::file_size(entry.path(), sizeEc));
            row += "|";
            row += timeEc ? "0" : std::to_string(static_cast<long long>(fs::last_write_time(entry.path(), timeEc).time_since_epoch().count()));
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

CAssetLibrary::~CAssetLibrary() = default;

void CAssetLibrary::SetTextMesh(CFreetypeTextMesh& textMesh)
{
    m_pTextMesh = &textMesh;
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
    m_Models.push_back(std::move(cubeAsset));

    CModelAsset sphereAsset;
    sphereAsset.m_Name = "Sphere";
    sphereAsset.m_Type = OBJECT_SPHERE;
    sphereAsset.m_IsProcedural = true;
    sphereAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshSphere(1.0f, seg, seg));
    };
    m_Models.push_back(std::move(sphereAsset));

    CModelAsset coneAsset;
    coneAsset.m_Name = "Cone";
    coneAsset.m_Type = OBJECT_CONE;
    coneAsset.m_IsProcedural = true;
    coneAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, seg));
    };
    m_Models.push_back(std::move(coneAsset));

    CModelAsset cylinderAsset;
    cylinderAsset.m_Name = "Cylinder";
    cylinderAsset.m_Type = OBJECT_CYLINDER;
    cylinderAsset.m_IsProcedural = true;
    cylinderAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, seg));
    };
    m_Models.push_back(std::move(cylinderAsset));

    CModelAsset hemisphereAsset;
    hemisphereAsset.m_Name = "HemiSphere";
    hemisphereAsset.m_Type = OBJECT_HEMISPHERE;
    hemisphereAsset.m_IsProcedural = true;
    hemisphereAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshHemiSphere(1.0f, seg, seg));
    };
    m_Models.push_back(std::move(hemisphereAsset));

    CModelAsset torusAsset;
    torusAsset.m_Name = "Torus";
    torusAsset.m_Type = OBJECT_TORUS;
    torusAsset.m_IsProcedural = true;
    torusAsset.pfnGenerator = [](int seg)
    {
        return LoadModelFromMesh(GenMeshTorus(1.0f, 1.0f, seg, seg));
    };
    m_Models.push_back(std::move(torusAsset));

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

    m_Models.push_back(std::move(textAsset));
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
    m_Textures.clear();
    m_Textures.push_back({ "None", {0} });

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
        m_Textures.push_back({ fs::relative(path, resourceDir).generic_string(), tex });
    }
}

void CAssetLibrary::UnloadTextures()
{
    std::unordered_set<unsigned int> releasedIds;
    for (auto& option : m_Textures)
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
    m_Textures.clear();
}

void CAssetLibrary::RefreshTextures(const std::string& projectPath, CScene* pScene)
{
    fs::path resourceDir = fs::path(projectPath) / "resources";
    if (!fs::exists(resourceDir))
    {
        fs::create_directories(resourceDir);
    }

    std::unordered_map<std::string, qc::Texture2D> oldByName;
    for (auto& option : m_Textures)
    {
        if (option.Texture.id != 0)
        {
            oldByName[option.Name] = option.Texture;
        }
    }

    std::vector<STextureOption> vNextOptions;
    vNextOptions.push_back({ "None", {0} });

    for (const auto& path : CollectResourceFiles(resourceDir))
    {
        if (!CTextureMetadataStore::IsImageFile(path))
        {
            continue;
        }

        const std::string textureName = fs::relative(path, resourceDir).generic_string();
        auto oldIt = oldByName.find(textureName);
        if (oldIt != oldByName.end())
        {
            vNextOptions.push_back({ textureName, oldIt->second });
            oldByName.erase(oldIt);
            continue;
        }

        qc::Texture2D tex = LoadTexture(path.string().c_str());
        vNextOptions.push_back({ textureName, tex });
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

    m_Textures = std::move(vNextOptions);
}

void CAssetLibrary::RefreshModels(const std::string& projectPath, CScene& scene)
{
    std::unordered_map<std::string, qc::Model> old;

    for (auto& asset : m_Models)
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
    vNext.reserve(m_Models.size());

    for (auto& asset : m_Models)
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

    for (const auto& path : CollectModelPaths(resourceDir))
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

    m_Models = std::move(vNext);

    for (auto& entity : scene.m_vEntities)
    {
        CMeshComponent* pMesh = entity.GetMeshComponent();
        if (!pMesh || pMesh->m_AssetName.empty())
        {
            continue;
        }
        pMesh->m_pAsset = nullptr;
        for (auto& asset : m_Models)
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
    RefreshTextures(projectPath, pScene);
    if (pScene)
    {
        RefreshModels(projectPath, *pScene);
    }
}

bool CAssetLibrary::PollResources(const std::string& projectPath, CScene& scene, double now)
{
    if (now - m_LastPollTime <= POLL_INTERVAL_SECONDS)
    {
        return false;
    }
    m_LastPollTime = now;

    const fs::path resourceDir = fs::path(projectPath) / "resources";
    if (!fs::exists(resourceDir))
    {
        return false;
    }

    const std::string currentSignature = BuildResourceSignature(resourceDir);
    if (currentSignature == m_LastResourceSignature)
    {
        return false;
    }

    m_LastResourceSignature = currentSignature;
    Refresh(projectPath, scene);
    return true;
}

void CAssetLibrary::Unload()
{
    m_Models.clear();
    UnloadTextures();
}

CModelAsset* CAssetLibrary::FindModelByName(const std::string& assetName)
{
    for (auto& asset : m_Models)
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
    for (const auto& asset : m_Models)
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
