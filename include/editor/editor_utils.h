#ifndef __EDITOR_UTILS_H__
#define __EDITOR_UTILS_H__

#include "entity.h"
#include <filesystem>
#include <string>

bool HasValidModelData(const Model& model);
Vec3 GetSceneDropPosition(Camera3D camera);
void ApplyNegativeScaleWinding(CEntity* pEntity);

#endif // __EDITOR_UTILS_H__
