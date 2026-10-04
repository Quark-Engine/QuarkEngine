#ifndef __EDITOR_HIERARCHY_UTILS_H__
#define __EDITOR_HIERARCHY_UTILS_H__

#include "../entity.h"
#include "../scene.h"
#include <vector>
#include <string>

std::vector<int> GetEntityChildren(const CScene& scene, int parentId);
std::vector<int> GetEntityDescendants(const CScene& scene, int entityId);
std::vector<int> GetRootEntities(const CScene& scene);

void MoveEntityToParent(CScene& scene, int entityId, int newParentId);
int CreateGroup(CScene& scene, const std::string& name, int parentId = -1);
void DeleteGroup(CScene& scene, int groupId, bool reparentToParent = true);
bool IsEntityGroup(const CEntity& entity);

#endif // __EDITOR_HIERARCHY_UTILS_H__
