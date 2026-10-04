#ifndef __ENGINE_SCENE_DOCUMENT_H__
#define __ENGINE_SCENE_DOCUMENT_H__
#include "../entity.h"
#include "../scene.h"
#include "component_factory_registry.h"
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

class CSceneDocument
{
public:
    static std::string Serialize(const CScene& scene, int indent = 4);

    static bool Deserialize(const std::string& document, CScene& scene, const CComponentFactoryRegistry& factories);

    static SSceneSnapshot CaptureSnapshot(const CScene& scene);
};

} // quark

#endif // __ENGINE_SCENE_DOCUMENT_H__