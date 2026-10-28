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
    std::string startScene = "scene.json";
    std::string iconPath;
    EProjectBuildProfile profile = EProjectBuildProfile::RELEASE;
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
    static void Save(const std::string& folderPath, const CScene& scene);
    static bool SaveSceneDocument(const std::string& sceneFilePath, const CScene& scene);
    static void SaveScene(const std::string& sceneFilePath, const CScene& scene);
    static bool Load(const std::string& folderPath, CScene& scene, CAssetLibrary& assets,
                     CLightRegistry& lights, const CComponentFactoryRegistry& factories);
};

#endif // __PROJECT_H__
