#ifndef __ENGINE_SCENE_DOCUMENT_H__
#define __ENGINE_SCENE_DOCUMENT_H__
#include "../entity.h"
#include "../scene.h"
#include "../editable_mesh.h"
#include "component_factory_registry.h"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>

namespace quark
{

struct SSceneSnapshot
{
    std::string Document;
    int Selected = -1;
    std::vector<int> vSelectedEntities;
};

struct SParsedSceneDocument
{
    nlohmann::json Document;
    struct SEditableMeshSnapshot
    {
        size_t EntityIndex = 0;
        CEditableMesh Mesh;
    };
    std::vector<SEditableMeshSnapshot> vEditableMeshes;
    bool IsValid = false;
};

class CSceneDocument
{
public:
    static std::string Serialize(const CScene& scene, int indent = 4);

    static SParsedSceneDocument Parse(const std::string& document);

    static bool Deserialize(const SParsedSceneDocument& document, CScene& scene,
        const CComponentFactoryRegistry& factories);

    static bool Deserialize(const std::string& document, CScene& scene, const CComponentFactoryRegistry& factories);

    static SSceneSnapshot CaptureSnapshot(const CScene& scene);
};

} // quark

#endif // __ENGINE_SCENE_DOCUMENT_H__