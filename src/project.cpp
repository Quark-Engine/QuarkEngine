#include "project.h"
#include "component.h"
#include "entity.h"
#include "lighting.h"
#include "version.h"
#include "engine/scene_document.h"
#include "engine/scene_runtime.h"
#include "QuarkCore/QuarkCore.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

using namespace qc;

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{

const char* QUARK_PROJECT_EXTENSION = ".quarkproj";

fs::path ProjectManifestPathForRoot(const fs::path& rootPath)
{
    const std::string rootName = rootPath.filename().string();
    const std::string manifestName = (rootName.empty() ? std::string("project") : rootName) + QUARK_PROJECT_EXTENSION;
    return rootPath / manifestName;
}

fs::path FindProjectManifest(const fs::path& rootPath)
{
    std::error_code ec;

    if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec))
    {
        return {};
    }

    const fs::path preferredManifest = ProjectManifestPathForRoot(rootPath);
    if (fs::exists(preferredManifest))
    {
        return preferredManifest;
    }

    for (const auto& entry : fs::directory_iterator(rootPath, ec))
    {
        if (ec)
        {
            break;
        }
        if (!entry.is_regular_file())
        {
            continue;
        }
        if (entry.path().extension() == QUARK_PROJECT_EXTENSION)
        {
            return entry.path();
        }
    }

    return {};
}

fs::path ResolveProjectRootPath(const fs::path& inputPath)
{
    if (inputPath.empty())
    {
        return {};
    }

    fs::path p = fs::absolute(inputPath);

    std::error_code ec;

    if (fs::is_directory(p, ec))
    {
        return p;
    }

    if (fs::is_regular_file(p, ec))
    {
        if (p.extension() == QUARK_PROJECT_EXTENSION)
        {
            return p.parent_path();
        }

        return p.parent_path();
    }

    return p;
}

bool WriteProjectManifest(const fs::path& rootPath, const std::string& sceneFile = "scene.json")
{
    fs::create_directories(rootPath);

    json manifest;
    manifest["format"] = "quark-project";
    manifest["version"] = QUARK_ENGINE_VERSION;
    manifest["name"] = rootPath.filename().string();
    manifest["scene"] = sceneFile;
    manifest["resources"] = "resources";

    const fs::path manifestPath = ProjectManifestPathForRoot(rootPath);
    std::ofstream manifestFile(manifestPath);
    if (!manifestFile.is_open())
    {
        return false;
    }
    manifestFile << manifest.dump(4);
    return true;
}

fs::path ResolveSceneJsonPath(const fs::path& inputPath)
{
    const fs::path rootPath = ResolveProjectRootPath(inputPath);
    if (rootPath.empty())
    {
        return {};
    }

    if (fs::is_regular_file(inputPath) && inputPath.extension() == QUARK_PROJECT_EXTENSION)
    {
        std::ifstream manifestFile(inputPath);
        if (manifestFile.is_open())
        {
            json manifest;
            try
            {
                manifestFile >> manifest;
                if (manifest.contains("scene") && manifest["scene"].is_string())
                {
                    return rootPath / manifest["scene"].get<std::string>();
                }
            }
            catch (...)
            {
            }
        }
    }

    const fs::path manifestPath = FindProjectManifest(rootPath);
    if (!manifestPath.empty())
    {
        std::ifstream manifestFile(manifestPath);
        if (manifestFile.is_open())
        {
            json manifest;
            try
            {
                manifestFile >> manifest;
                if (manifest.contains("scene") && manifest["scene"].is_string())
                {
                    return rootPath / manifest["scene"].get<std::string>();
                }
            }
            catch (...)
            {
            }
        }
    }

    return rootPath / "scene.json";
}

} // anonymous

std::string CProjectService::ResolveRoot(const std::string& path)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(path));
    if (rootPath.empty())
    {
        return {};
    }
    return fs::absolute(rootPath).string();
}

bool CProjectService::IsValid(const std::string& path)
{
    const fs::path inputPath(path);
    const fs::path rootPath = ResolveProjectRootPath(inputPath);
    if (rootPath.empty() || !fs::exists(rootPath))
    {
        return false;
    }
    return fs::exists(ResolveSceneJsonPath(inputPath));
}

void CProjectService::CreateNew(const std::string& folderPath, CScene& scene)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(folderPath));
    fs::create_directories(rootPath);
    fs::create_directories(rootPath / "resources");

    json j;
    j["entities"] = json::array();

    std::ofstream f(rootPath / "scene.json");
    f << j.dump(4);
    WriteProjectManifest(rootPath);

    scene.ReleaseResources();
}

void CProjectService::Save(const std::string& folderPath, const CScene& scene)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(folderPath));
    if (rootPath.empty())
    {
        return;
    }
    SaveScene((rootPath / "scene.json").string(), scene);
}

void CProjectService::SaveScene(const std::string& sceneFilePath, const CScene& scene)
{
    const fs::path scenePath = fs::absolute(fs::path(sceneFilePath));
    const fs::path rootPath = scenePath.parent_path();
    if (rootPath.empty())
    {
        TraceLog(LogLevel::Error, "PROJECT", "Failed to save scene: empty parent directory");
        return;
    }
    fs::create_directories(rootPath / "resources");

    std::ofstream f(scenePath);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Failed to open scene file for writing: %s", sceneFilePath.c_str()));
        return;
    }
    f << quark::CSceneDocument::Serialize(scene);
    f.close();
    WriteProjectManifest(rootPath, scenePath.filename().string());
}

bool CProjectService::Load(const std::string& folderPath, CScene& scene, CAssetLibrary& assets,
                           CLightRegistry& lights, const CComponentFactoryRegistry& factories)
{
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("Loading project: %s", folderPath.c_str()));
    const fs::path rootPath = ResolveProjectRootPath(fs::path(folderPath));
    const fs::path jsonPath = ResolveSceneJsonPath(fs::path(folderPath));
    if (!fs::exists(jsonPath))
    {
        return false;
    }
    const fs::path manifestPath = FindProjectManifest(rootPath);
    std::string projectName = rootPath.filename().string();
    std::string projectVersion = "unknown";

    if (!manifestPath.empty())
    {
        std::ifstream mf(manifestPath);
        if (mf.is_open())
        {
            json manifest;
            try
            {
                mf >> manifest;
                if (manifest.contains("name") && manifest["name"].is_string())
                {
                    projectName = manifest["name"].get<std::string>();
                }
                if (manifest.contains("version") && manifest["version"].is_string())
                {
                    projectVersion = manifest["version"].get<std::string>();
                }
            }
            catch (...)
            {
            }
        }
    }

    TraceLog(LogLevel::Info, "PROJECT", "=== Loading project ===");
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Name:    %s", projectName.c_str()));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Version: %s", projectVersion.c_str()));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Scene:   %s", jsonPath.string().c_str()));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Engine:  %s", QUARK_ENGINE_VERSION));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Root:    %s", rootPath.string().c_str()));
    TraceLog(LogLevel::Info, "PROJECT", "======================");

    scene.ReleaseResources();
    lights.Reset();
    assets.Refresh(rootPath.string(), &scene);

    std::ifstream f(jsonPath);
    std::stringstream document;
    document << f.rdbuf();
    if (!quark::CSceneDocument::Deserialize(document.str(), scene, factories))
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Malformed scene file: %s", jsonPath.string().c_str()));
        return false;
    }

    for (auto& e : scene.m_vEntities)
    {
        if (!e.GetTransformComponent())
        {
            e.GetComponents()->AddComponent(std::make_shared<CTransformComponent>());
        }
        if (e.GetLightComponent())
        {
            continue;
        }
        if (!e.GetMeshComponent())
        {
            e.GetComponents()->AddComponent(std::make_shared<CMeshComponent>());
        }
        if (!e.GetMaterialComponent())
        {
            e.GetComponents()->AddComponent(std::make_shared<CMaterialComponent>());
        }
    }

    quark::CSceneRuntime::RestoreSceneEntityModels(scene, assets);
    quark::CSceneRuntime::ResetSceneLightRuntime(scene, lights);

    return true;
}

std::string CProjectService::GetVersion(const std::string& path)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(path));
    const fs::path manifestPath = FindProjectManifest(rootPath);
    if (manifestPath.empty())
    {
        return "";
    }

    std::ifstream f(manifestPath);
    if (!f.is_open())
    {
        return "";
    }

    json manifest;
    try
    {
        f >> manifest;
        if (manifest.contains("version"))
        {
            if (manifest["version"].is_string())
            {
                return manifest["version"].get<std::string>();
            }
            return manifest["version"].dump();
        }
    }
    catch (...)
    {
    }
    return "";
}
