#ifndef __PROJECT_H__
#define __PROJECT_H__
#include <string>
#include "scene.h"
#include "assets/asset_library.h"
#include "engine/component_factory_registry.h"
#include "lighting.h"

class CProjectService
{
public:
    static std::string ResolveRoot(const std::string& path);
    static std::string GetVersion(const std::string& path);
    static bool IsValid(const std::string& path);
    static void CreateNew(const std::string& folderPath, CScene& scene);
    static void Save(const std::string& folderPath, const CScene& scene);
    static void SaveScene(const std::string& sceneFilePath, const CScene& scene);
    static bool Load(const std::string& folderPath, CScene& scene, CAssetLibrary& assets,
                     CLightRegistry& lights, const CComponentFactoryRegistry& factories);
};

#endif // __PROJECT_H__
