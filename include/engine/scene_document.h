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
        std::string m_Document;
        int m_Selected = -1;
        std::vector<int> m_vSelectedEntities;
    };

    struct SParsedSceneDocument
    {
        nlohmann::json m_Document;
        struct SEditableMeshSnapshot
        {
            size_t m_EntityIndex = 0;
            CEditableMesh m_Mesh;
        };
        std::vector<SEditableMeshSnapshot> m_vEditableMeshes;
        bool m_IsValid = false;
    };

    class CSceneDocument
    {
    public:
        static std::string Serialize(const CScene& scene, int indent = 4);

        static SParsedSceneDocument Parse(const std::string& document);

        static bool Deserialize(const SParsedSceneDocument& document, CScene& scene,
                                const CComponentFactoryRegistry& factories);

        static bool Deserialize(const std::string& document, CScene& scene, const CComponentFactoryRegistry& factories);

        static std::string SerializePrefab(const CScene& scene, int rootEntityIndex, int indent = 4);

        static SParsedSceneDocument ParsePrefab(const std::string& document);

        static bool InstantiatePrefab(const SParsedSceneDocument& parsedPrefab, CScene& scene,
                                      const CComponentFactoryRegistry& factories, int parentEntityIndex = -1,
                                      std::vector<int>* pvOutCreatedIndices = nullptr);

        static bool InstantiatePrefab(const std::string& document, CScene& scene,
                                      const CComponentFactoryRegistry& factories, int parentEntityIndex = -1,
                                      std::vector<int>* pvOutCreatedIndices = nullptr);

        static SSceneSnapshot CaptureSnapshot(const CScene& scene);
    };

} // quark

#endif // __ENGINE_SCENE_DOCUMENT_H__
