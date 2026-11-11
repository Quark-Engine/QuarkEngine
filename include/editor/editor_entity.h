#ifndef __EDITOR_ENTITY_H__
#define __EDITOR_ENTITY_H__

#include "assets/asset_library.h"
#include "engine/component_factory_registry.h"
#include "entity.h"
#include "scene.h"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class CEntityFactory
{
public:
    static CEntity FromAsset(CScene& scene, CModelAsset& asset);

    static CEntity Light(CScene& scene, int parentIndex);
    static CEntity Camera(CScene& scene, int parentIndex);
    static CEntity UiWidget(CScene& scene, EGuiWidgetType widgetType, int parentIndex = -1);

    static void AssignName(CEntity& entity, const char* pNewName);

    static bool SavePrefab(const CScene& scene, int rootEntityIndex, const fs::path& directoryPath,
                           fs::path* pOutFilePath = nullptr);
    static void SavePrefab(CEntity entity, const fs::path path);

    static std::vector<int> InstantiatePrefab(CScene& scene, CAssetLibrary& assets,
                                              const fs::path& filename,
                                              const CComponentFactoryRegistry& factories,
                                              int parentIndex = -1);

    static CEntity FromPrefab(CScene& scene, const CAssetLibrary& assets, const fs::path filename,
                              const CComponentFactoryRegistry& factories);
};

#endif // __EDITOR_ENTITY_H__
