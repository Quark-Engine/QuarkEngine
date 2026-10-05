#ifndef __ENGINE_SCENE_RUNTIME_H__
#define __ENGINE_SCENE_RUNTIME_H__
#include "../assets/asset_library.h"
#include "../entity.h"
#include "../lighting.h"
#include "../scene.h"

#include <optional>
#include <vector>

namespace quark
{

class CSceneRuntime
{
public:
    static void RestoreEntityMaterial(CEntity& entity, const CAssetLibrary& assets);

    static void RestoreSceneEntityModels(CScene& scene, CAssetLibrary& assets,
        CScene* pPreviousScene = nullptr,
        const std::vector<std::optional<SEditableMeshBuildData>>* pvEditableMeshBuildData = nullptr);
    static void RestoreSceneEntityMaterials(CScene& scene, const CAssetLibrary& assets);

    static void ResetSceneLightRuntime(CScene& scene, CLightRegistry& lights);
};

} // quark

#endif // __ENGINE_SCENE_RUNTIME_H__