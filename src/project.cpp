#include "project.h"
#include "component.h"
#include "entity.h"
#include "lighting.h"
#include "version.h"
#include "engine/scene_document.h"
#include "engine/scene_runtime.h"
#include "QuarkCore/QuarkCore.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
namespace fs = std::filesystem;
using Json = nlohmann::json;

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

    Json ReadProjectManifest(const fs::path& rootPath)
    {
        const fs::path manifestPath = FindProjectManifest(rootPath);
        if (manifestPath.empty())
        {
            return Json::object();
        }

        std::ifstream manifestFile(manifestPath);
        if (!manifestFile.is_open())
        {
            return Json::object();
        }

        try
        {
            Json manifest;
            manifestFile >> manifest;
            return manifest.is_object() ? manifest : Json::object();
        }
        catch (...)
        {
            return Json::object();
        }
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

        Json manifest = ReadProjectManifest(rootPath);
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

        if (fs::is_regular_file(inputPath) && inputPath.extension() == ".json")
        {
            return fs::absolute(inputPath);
        }

        if (fs::is_regular_file(inputPath) && inputPath.extension() == QUARK_PROJECT_EXTENSION)
        {
            std::ifstream manifestFile(inputPath);
            if (manifestFile.is_open())
            {
                Json manifest;
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
                Json manifest;
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

std::string CProjectService::GetActiveScenePath(const std::string& projectPath)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(projectPath));
    if (rootPath.empty())
    {
        return {};
    }
    return ResolveSceneJsonPath(rootPath).string();
}

std::vector<std::string> CProjectService::GetSceneFiles(const std::string& projectPath)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(projectPath));
    std::vector<std::string> vSceneFiles;
    if (rootPath.empty())
    {
        return vSceneFiles;
    }

    std::error_code error;
    fs::recursive_directory_iterator iterator(rootPath, error);
    const fs::recursive_directory_iterator end;
    while (!error && iterator != end)
    {
        const fs::path relativePath = fs::relative(iterator->path(), rootPath, error);
        if (error)
        {
            break;
        }
        if (iterator->is_directory(error) &&
            (relativePath == "Build" || relativePath == "resources" || relativePath == ".git"))
        {
            iterator.disable_recursion_pending();
        }
        else if (iterator->is_regular_file(error) && iterator->path().extension() == ".json" &&
                 relativePath != "build_settings.json")
        {
            vSceneFiles.push_back(relativePath.generic_string());
        }
        iterator.increment(error);
    }
    std::sort(vSceneFiles.begin(), vSceneFiles.end());
    return vSceneFiles;
}

bool CProjectService::LoadBuildSettings(const std::string& projectPath,
                                        SProjectBuildSettings& settings)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(projectPath));
    if (rootPath.empty())
    {
        return false;
    }

    const Json manifest = ReadProjectManifest(rootPath);
    settings.m_StartScene = fs::relative(
                                ResolveSceneJsonPath(rootPath), rootPath)
                                .generic_string();
    settings.m_IconPath.clear();
    settings.m_ProductName = rootPath.filename().string();
    settings.m_CompanyName = "DefaultCompany";
    settings.m_Version = "1.0.0";
    settings.m_WindowWidth = 1280;
    settings.m_WindowHeight = 720;
    settings.m_Fullscreen = false;
    settings.m_ResizableWindow = true;
    settings.m_Gravity = { 0.0f, -9.81f, 0.0f };
    settings.m_FixedTimestep = 0.02f;
    settings.m_SkyboxEnabled = true;
    settings.m_ToneMappingEnabled = true;
    settings.m_Exposure = 1.05f;
    settings.m_BloomStrength = 0.25f;
    settings.m_ShadowsEnabled = true;
    settings.m_ShadowBias = 0.005f;
    settings.m_ShadowFilterQuality = 2;
    settings.m_VsyncEnabled = false;
    std::string profile = "Release";
    if (manifest.contains("build_start_scene") && manifest["build_start_scene"].is_string())
    {
        settings.m_StartScene = manifest["build_start_scene"].get<std::string>();
    }
    if (manifest.contains("build_icon") && manifest["build_icon"].is_string())
    {
        settings.m_IconPath = manifest["build_icon"].get<std::string>();
    }
    if (manifest.contains("build_profile") && manifest["build_profile"].is_string())
    {
        profile = manifest["build_profile"].get<std::string>();
    }
    if (manifest.contains("product_name") && manifest["product_name"].is_string())
    {
        const std::string product = manifest["product_name"].get<std::string>();
        if (!product.empty())
        {
            settings.m_ProductName = product;
        }
    }
    if (manifest.contains("company_name") && manifest["company_name"].is_string())
    {
        settings.m_CompanyName = manifest["company_name"].get<std::string>();
    }
    if (manifest.contains("product_version") && manifest["product_version"].is_string())
    {
        settings.m_Version = manifest["product_version"].get<std::string>();
    }
    if (manifest.contains("window_width") && manifest["window_width"].is_number_integer())
    {
        settings.m_WindowWidth = std::clamp(manifest["window_width"].get<int>(), 320, 7680);
    }
    if (manifest.contains("window_height") && manifest["window_height"].is_number_integer())
    {
        settings.m_WindowHeight = std::clamp(manifest["window_height"].get<int>(), 240, 4320);
    }
    if (manifest.contains("fullscreen") && manifest["fullscreen"].is_boolean())
    {
        settings.m_Fullscreen = manifest["fullscreen"].get<bool>();
    }
    if (manifest.contains("resizable_window") && manifest["resizable_window"].is_boolean())
    {
        settings.m_ResizableWindow = manifest["resizable_window"].get<bool>();
    }
    if (manifest.contains("physics_gravity") && manifest["physics_gravity"].is_array() &&
        manifest["physics_gravity"].size() == 3)
    {
        settings.m_Gravity.x = manifest["physics_gravity"][0].get<float>();
        settings.m_Gravity.y = manifest["physics_gravity"][1].get<float>();
        settings.m_Gravity.z = manifest["physics_gravity"][2].get<float>();
    }
    if (manifest.contains("physics_fixed_timestep") && manifest["physics_fixed_timestep"].is_number())
    {
        settings.m_FixedTimestep = std::clamp(manifest["physics_fixed_timestep"].get<float>(), 0.002f, 0.1f);
    }
    if (manifest.contains("graphics_skybox_enabled") && manifest["graphics_skybox_enabled"].is_boolean())
    {
        settings.m_SkyboxEnabled = manifest["graphics_skybox_enabled"].get<bool>();
    }
    if (manifest.contains("graphics_tone_mapping_enabled") && manifest["graphics_tone_mapping_enabled"].is_boolean())
    {
        settings.m_ToneMappingEnabled = manifest["graphics_tone_mapping_enabled"].get<bool>();
    }
    if (manifest.contains("graphics_exposure") && manifest["graphics_exposure"].is_number())
    {
        settings.m_Exposure = std::clamp(manifest["graphics_exposure"].get<float>(), 0.2f, 3.0f);
    }
    if (manifest.contains("graphics_bloom_strength") && manifest["graphics_bloom_strength"].is_number())
    {
        settings.m_BloomStrength = std::clamp(manifest["graphics_bloom_strength"].get<float>(), 0.0f, 1.5f);
    }
    if (manifest.contains("graphics_shadows_enabled") && manifest["graphics_shadows_enabled"].is_boolean())
    {
        settings.m_ShadowsEnabled = manifest["graphics_shadows_enabled"].get<bool>();
    }
    if (manifest.contains("graphics_shadow_bias") && manifest["graphics_shadow_bias"].is_number())
    {
        settings.m_ShadowBias = std::clamp(manifest["graphics_shadow_bias"].get<float>(), 0.0001f, 0.05f);
    }
    if (manifest.contains("graphics_shadow_filter_quality") && manifest["graphics_shadow_filter_quality"].is_number_integer())
    {
        settings.m_ShadowFilterQuality = std::clamp(manifest["graphics_shadow_filter_quality"].get<int>(), 0, 2);
    }
    if (manifest.contains("graphics_vsync_enabled") && manifest["graphics_vsync_enabled"].is_boolean())
    {
        settings.m_VsyncEnabled = manifest["graphics_vsync_enabled"].get<bool>();
    }
    settings.m_Profile = profile == "Debug"
        ? EProjectBuildProfile::DEBUG
        : EProjectBuildProfile::RELEASE;

    const std::vector<std::string> vSceneFiles = GetSceneFiles(rootPath.string());
    if (std::find(vSceneFiles.begin(), vSceneFiles.end(), settings.m_StartScene) == vSceneFiles.end())
    {
        settings.m_StartScene = "scene.json";
        if (std::find(vSceneFiles.begin(), vSceneFiles.end(), settings.m_StartScene) == vSceneFiles.end() &&
            !vSceneFiles.empty())
        {
            settings.m_StartScene = vSceneFiles.front();
        }
    }
    return true;
}

bool CProjectService::SaveBuildSettings(const std::string& projectPath,
                                        const SProjectBuildSettings& settings)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(projectPath));
    if (rootPath.empty())
    {
        return false;
    }

    const std::vector<std::string> vSceneFiles = GetSceneFiles(rootPath.string());
    if (std::find(vSceneFiles.begin(), vSceneFiles.end(), settings.m_StartScene) == vSceneFiles.end())
    {
        return false;
    }
    if (!settings.m_IconPath.empty())
    {
        const fs::path iconPath(settings.m_IconPath);
        if (iconPath.is_absolute() || iconPath.has_root_name())
        {
            return false;
        }
        for (const fs::path& component : iconPath)
        {
            if (component == ".." || component == ".")
            {
                return false;
            }
        }
        if (!fs::is_regular_file(rootPath / iconPath))
        {
            return false;
        }
    }

    Json manifest = ReadProjectManifest(rootPath);
    manifest["format"] = "quark-project";
    manifest["version"] = QUARK_ENGINE_VERSION;
    manifest["name"] = rootPath.filename().string();
    manifest["scene"] = fs::relative(ResolveSceneJsonPath(rootPath), rootPath).generic_string();
    manifest["resources"] = "resources";
    manifest["build_start_scene"] = settings.m_StartScene;
    manifest["build_icon"] = settings.m_IconPath;
    manifest["build_profile"] = settings.m_Profile == EProjectBuildProfile::DEBUG
        ? "Debug"
        : "Release";
    manifest["product_name"] = settings.m_ProductName.empty()
        ? rootPath.filename().string()
        : settings.m_ProductName;
    manifest["company_name"] = settings.m_CompanyName;
    manifest["product_version"] = settings.m_Version;
    manifest["window_width"] = std::clamp(settings.m_WindowWidth, 320, 7680);
    manifest["window_height"] = std::clamp(settings.m_WindowHeight, 240, 4320);
    manifest["fullscreen"] = settings.m_Fullscreen;
    manifest["resizable_window"] = settings.m_ResizableWindow;
    manifest["physics_gravity"] = { settings.m_Gravity.x, settings.m_Gravity.y, settings.m_Gravity.z };
    manifest["physics_fixed_timestep"] = std::clamp(settings.m_FixedTimestep, 0.002f, 0.1f);
    manifest["graphics_skybox_enabled"] = settings.m_SkyboxEnabled;
    manifest["graphics_tone_mapping_enabled"] = settings.m_ToneMappingEnabled;
    manifest["graphics_exposure"] = std::clamp(settings.m_Exposure, 0.2f, 3.0f);
    manifest["graphics_bloom_strength"] = std::clamp(settings.m_BloomStrength, 0.0f, 1.5f);
    manifest["graphics_shadows_enabled"] = settings.m_ShadowsEnabled;
    manifest["graphics_shadow_bias"] = std::clamp(settings.m_ShadowBias, 0.0001f, 0.05f);
    manifest["graphics_shadow_filter_quality"] = std::clamp(settings.m_ShadowFilterQuality, 0, 2);
    manifest["graphics_vsync_enabled"] = settings.m_VsyncEnabled;

    std::ofstream manifestFile(ProjectManifestPathForRoot(rootPath), std::ios::trunc);
    if (!manifestFile.is_open())
    {
        return false;
    }
    manifestFile << manifest.dump(4);
    return static_cast<bool>(manifestFile);
}

void CProjectService::CreateNew(const std::string& folderPath, CScene& scene)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(folderPath));
    fs::create_directories(rootPath);
    fs::create_directories(rootPath / "resources");

    scene.ReleaseResources();
    scene.m_vEntities.clear();

    CEntity cube;
    cube.m_Id = 0;
    cube.m_Name = "Cube";
    cube.GetMeshComponent()->m_Type = OBJECT_CUBE;
    cube.GetMeshComponent()->m_AssetName = "Cube";
    cube.GetComponents()->AddComponent(std::make_shared<CMaterialComponent>());
    scene.m_vEntities.push_back(std::move(cube));

    CEntity light;
    light.m_Id = 1;
    light.m_Name = "Light";
    light.GetComponents()->RemoveComponent(1);
    light.GetTransformComponent()->m_Position = { -3.0f, 4.0f, 2.0f };
    light.GetComponents()->AddComponent(std::make_shared<CLightComponent>());
    scene.m_vEntities.push_back(std::move(light));

    CEntity camera;
    camera.m_Id = 2;
    camera.m_Name = "Camera";
    camera.GetComponents()->RemoveComponent(1);
    camera.GetTransformComponent()->m_Position = { 0.0f, 1.5f, 6.0f };
    camera.GetTransformComponent()->m_Rotation = { -12.0f, 0.0f, 0.0f };
    camera.GetComponents()->AddComponent(std::make_shared<CCameraComponent>());
    scene.m_vEntities.push_back(std::move(camera));
    scene.m_Selected = 0;
    scene.m_vSelectedEntities = { 0 };

    std::ofstream f(rootPath / "scene.json");
    f << quark::CSceneDocument::Serialize(scene);
    WriteProjectManifest(rootPath);
}

bool CProjectService::CreateEmptySceneFile(const std::string& sceneFilePath)
{
    if (sceneFilePath.empty())
    {
        return false;
    }

    const fs::path targetPath = fs::absolute(fs::path(sceneFilePath));
    std::error_code ec;
    if (!targetPath.parent_path().empty())
    {
        fs::create_directories(targetPath.parent_path(), ec);
    }

    CScene defaultScene;

    CEntity light;
    light.m_Id = 0;
    light.m_Name = "Light";
    light.GetComponents()->RemoveComponent(1);
    light.GetTransformComponent()->m_Position = { -3.0f, 4.0f, 2.0f };
    light.GetComponents()->AddComponent(std::make_shared<CLightComponent>());
    defaultScene.m_vEntities.push_back(std::move(light));

    CEntity camera;
    camera.m_Id = 1;
    camera.m_Name = "Camera";
    camera.GetComponents()->RemoveComponent(1);
    camera.GetTransformComponent()->m_Position = { 0.0f, 1.5f, 6.0f };
    camera.GetTransformComponent()->m_Rotation = { -12.0f, 0.0f, 0.0f };
    camera.GetComponents()->AddComponent(std::make_shared<CCameraComponent>());
    defaultScene.m_vEntities.push_back(std::move(camera));
    defaultScene.m_Selected = -1;

    return SaveSceneDocument(targetPath.string(), defaultScene);
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

    if (!SaveSceneDocument(scenePath.string(), scene))
    {
        return;
    }
    WriteProjectManifest(rootPath, scenePath.filename().string());
}

bool CProjectService::SaveSceneDocument(const std::string& sceneFilePath, const CScene& scene)
{
    const fs::path scenePath = fs::absolute(fs::path(sceneFilePath));
    const fs::path rootPath = scenePath.parent_path();
    if (rootPath.empty())
    {
        TraceLog(LogLevel::Error, "PROJECT", "Failed to save scene: empty parent directory");
        return false;
    }

    std::ofstream f(scenePath);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Failed to open scene file for writing: %s", sceneFilePath.c_str()));
        return false;
    }
    f << quark::CSceneDocument::Serialize(scene);
    f.close();
    if (!f)
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Failed to write scene file: %s", sceneFilePath.c_str()));
        return false;
    }
    return true;
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
            Json manifest;
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
        if (e.HasUiComponent())
        {
            const bool isCanvas = e.GetCanvasComponent() != nullptr;
            e.GetComponents()->RemoveMeshAndMaterialForUi(isCanvas);
            continue;
        }
        if (!e.GetTransformComponent())
        {
            e.GetComponents()->AddComponent(std::make_shared<CTransformComponent>());
        }
        if (e.GetLightComponent() || e.GetCameraComponent())
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

bool CProjectService::LoadSceneFile(const std::string& projectPath, const std::string& sceneFilePath,
                                    CScene& scene, CAssetLibrary& assets,
                                    CLightRegistry& lights, const CComponentFactoryRegistry& factories)
{
    const fs::path rootPath = ResolveProjectRootPath(fs::path(projectPath));
    const fs::path jsonPath = fs::path(sceneFilePath);
    if (!fs::is_regular_file(jsonPath))
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Scene file not found: %s", sceneFilePath.c_str()));
        return false;
    }

    std::ifstream file(jsonPath);
    if (!file.is_open())
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Failed to open scene file: %s", sceneFilePath.c_str()));
        return false;
    }

    std::stringstream document;
    document << file.rdbuf();

    CScene loadedScene;
    loadedScene.m_pComponentRegistry = scene.m_pComponentRegistry;
    if (!quark::CSceneDocument::Deserialize(document.str(), loadedScene, factories))
    {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Malformed scene file: %s", jsonPath.string().c_str()));
        return false;
    }

    scene.ReleaseResources();
    lights.Reset();
    scene.m_vEntities = std::move(loadedScene.m_vEntities);
    scene.m_Selected = -1;
    scene.m_vSelectedEntities.clear();
    if (!rootPath.empty())
    {
        assets.Refresh(rootPath.string(), &scene);
    }

    for (auto& entity : scene.m_vEntities)
    {
        if (entity.HasUiComponent())
        {
            const bool isCanvas = entity.GetCanvasComponent() != nullptr;
            entity.GetComponents()->RemoveMeshAndMaterialForUi(isCanvas);
            continue;
        }
        if (!entity.GetTransformComponent())
        {
            entity.GetComponents()->AddComponent(std::make_shared<CTransformComponent>());
        }
        if (entity.GetLightComponent() || entity.GetCameraComponent())
        {
            continue;
        }
        if (!entity.GetMeshComponent())
        {
            entity.GetComponents()->AddComponent(std::make_shared<CMeshComponent>());
        }
        if (!entity.GetMaterialComponent())
        {
            entity.GetComponents()->AddComponent(std::make_shared<CMaterialComponent>());
        }
    }

    quark::CSceneRuntime::RestoreSceneEntityModels(scene, assets);
    quark::CSceneRuntime::ResetSceneLightRuntime(scene, lights);
    scene.BumpTransformRevision();

    TraceLog(LogLevel::Info, "PROJECT", TextFormat("Loaded scene: %s", jsonPath.string().c_str()));
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

    Json manifest;
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
