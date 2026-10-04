#ifndef __EDITOR_UTILS_H__
#define __EDITOR_UTILS_H__

#include "entity.h"
#include <filesystem>
#include <string>

bool HasValidModelData(const qc::Model& model);
qc::Vec3 GetSceneDropPosition(qc::Camera3D camera);
void ApplyNegativeScaleWinding(CEntity* pEntity);

#endif // __EDITOR_UTILS_H__
