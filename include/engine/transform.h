#ifndef __ENGINE_TRANSFORM_H__
#define __ENGINE_TRANSFORM_H__
#include "../entity.h"
#include "../scene.h"
#include <vector>

namespace quark
{

Mat4 ComposeLocal(const CTransformComponent& transform);
Mat4 ComposeLocal(const CEntity& entity);

int IndexOfEntity(const CScene& scene, const CEntity& entity);

Mat4 ComposeWorld(const CScene& scene, int entityIndex);
Mat4 ComposeWorld(const CScene& scene, const CEntity& entity);

Mat4 ComposeMeshWorld(const CScene& scene, const CEntity& entity);

Mat4 ParentWorld(const CScene& scene, const CEntity& entity);

bool TryInvertAffine(const Mat4& matrix, Mat4& inverse);
bool HasNegativeDeterminant(const Mat4& matrix);

void DecomposeLocal(const Mat4& parentWorld, const Mat4& world, CTransformComponent& out);
void DecomposeWorld(const Mat4& world, CTransformComponent& out);

Mat4 ComposeWorld(const CScene& scene, int entityIndex, std::vector<int>& vStack);

} // quark

#endif // __ENGINE_TRANSFORM_H__
