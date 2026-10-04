#ifndef __ENGINE_TRANSFORM_H__
#define __ENGINE_TRANSFORM_H__
#include "../entity.h"
#include "../scene.h"
#include <vector>

namespace quark
{

qc::Mat4 ComposeLocal(const CTransformComponent& transform);
qc::Mat4 ComposeLocal(const CEntity& entity);

int IndexOfEntity(const CScene& scene, const CEntity& entity);

qc::Mat4 ComposeWorld(const CScene& scene, int entityIndex);
qc::Mat4 ComposeWorld(const CScene& scene, const CEntity& entity);

qc::Mat4 ComposeMeshWorld(const CScene& scene, const CEntity& entity);

qc::Mat4 ParentWorld(const CScene& scene, const CEntity& entity);

void DecomposeLocal(const qc::Mat4& parentWorld, const qc::Mat4& world, CTransformComponent& out);
void DecomposeWorld(const qc::Mat4& world, CTransformComponent& out);

qc::Mat4 ComposeWorld(const CScene& scene, int entityIndex, std::vector<int>& vStack);

} // quark

#endif // __ENGINE_TRANSFORM_H__
