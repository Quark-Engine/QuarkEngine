#ifndef __SCENE_H__
#define __SCENE_H__
#include <vector>
#include "entity.h"
#include "lighting.h"
#include <memory>
#include <string>

class CScene
{
public:
    CEntity* GetSelected();
    bool IsSelected(int entityIndex) const;
    void SelectEntity(int entityIndex, bool additive);
    std::string MakeUniqueName(const std::string& baseName) const;
    std::string MakeDefaultNameFor(const CEntity& entity) const;
    void ReleaseResources();

    std::vector<CEntity> m_vEntities;

    int m_Selected = -1;
    std::vector<int> m_vSelectedEntities;
};

#endif // __SCENE_H__