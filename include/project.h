#ifndef __PROJECT_H__
#define __PROJECT_H__

#include <string>
#include <vector>
#include "scene.h"
#include "assets/asset_library.h"
#include "engine/component_factory_registry.h"
#include "lighting.h"

enum class EProjectBuildProfile
{
    DEBUG,
    RELEASE
};

struct SProjectBuildSettings
{
    std::string m_StartScene = "scene.json";
    std::string m_IconPath;
    EProjectBuildProfile m_Profile = EProjectBuildProfile::RELEASE;
    std::string m_ProductName;
    std::string m_CompanyName = "DefaultCompany";
    std::string m_Version = "1.0.0";
    int m_WindowWidth = 1280;
    int m_WindowHeight = 720;
    bool m_Fullscreen = false;
    bool m_ResizableWindow = true;
    Vec3 m_Gravity = { 0.0f, -9.81f, 0.0f };
    float m_FixedTimestep = 0.02f;
    bool m_SkyboxEnabled = true;
    bool m_ToneMappingEnabled = true;
    float m_Exposure = 1.05f;
    float m_BloomStrength = 0.25f;
    bool m_ShadowsEnabled = true;
    float m_ShadowBias = 0.005f;
    int m_ShadowFilterQuality = 2;
    bool m_VsyncEnabled = false;
};

class CProjectService
{
public:
    static std::string ResolveRoot(const std::string& path);
    static std::string GetVersion(const std::string& path);
    static bool IsValid(const std::string& path);
    static std::string GetActiveScenePath(const std::string& projectPath);
    static std::vector<std::string> GetSceneFiles(const std::string& projectPath);
    static bool LoadBuildSettings(const std::string& projectPath,
                                  SProjectBuildSettings& settings);
    static bool SaveBuildSettings(const std::string& projectPath,
                                  const SProjectBuildSettings& settings);
    static void CreateNew(const std::string& folderPath, CScene& scene);
    static bool CreateEmptySceneFile(const std::string& sceneFilePath);
    static void Save(const std::string& folderPath, const CScene& scene);
    static bool SaveSceneDocument(const std::string& sceneFilePath, const CScene& scene);
    static void SaveScene(const std::string& sceneFilePath, const CScene& scene);
    static bool LoadSceneFile(const std::string& projectPath, const std::string& sceneFilePath,
                              CScene& scene, CAssetLibrary& assets,
                              CLightRegistry& lights, const CComponentFactoryRegistry& factories);
    static bool Load(const std::string& folderPath, CScene& scene, CAssetLibrary& assets,
                     CLightRegistry& lights, const CComponentFactoryRegistry& factories);
};

#endif // __PROJECT_H__
