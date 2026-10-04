#ifndef __EDITOR_ENTITY_H__
#define __EDITOR_ENTITY_H__

#include "assets/asset_library.h"
#include "engine/component_factory_registry.h"
#include "entity.h"
#include "scene.h"

#include <filesystem>

namespace fs = std::filesystem;

class CEntityFactory
{
public:
    static CEntity FromAsset(CScene& scene, CModelAsset& asset);

    static CEntity Light(CScene& scene, int parentIndex);

    static void AssignName(CEntity& entity, const char* pNewName);

    static void SavePrefab(CEntity entity, const fs::path path);

    static CEntity FromPrefab(CScene& scene, const CAssetLibrary& assets, const fs::path filename,
                              const CComponentFactoryRegistry& factories);
};

#endif // __EDITOR_ENTITY_H__