#ifndef __EDITOR_EDITOR_MESH_EDIT_H__
#define __EDITOR_EDITOR_MESH_EDIT_H__

#include "QuarkCore/QuarkCore.hpp"
#include "../scene.h"

class CEditor;
class CFreetypeTextMesh;

class CMeshEditor
{
public:
    static void SyncState(CEditor& editor);

    static bool GetSelectedVertexIndex(const CEntity& entity, int meshIndex, int triangleIndex,
        int vertexCorner, int& outVertexIndex);
    static Vec3 GetVertexWorldPosition(const CScene& scene, const CEntity& entity, int meshIndex,
        int vertexIndex);
    static bool SetVertexWorldPosition(const CScene& scene, CEntity& entity, int meshIndex,
        int vertexIndex, const Vec3& worldPosition);

    static void ResetModel(CEntity& entity, const CFreetypeTextMesh& textMesh);

    static void DrawOverlay(CEditor& editor, const Camera3D& camera);

private:
    static bool GetSelectedTriangleVertices(const CEntity& entity, int meshIndex, int triangleIndex,
        int aOutIndices[3]);
    static Vec3 GetVertexLocalPosition(const CEntity& entity, int meshIndex, int vertexIndex);
    static bool SetVertexLocalPosition(CEntity& entity, int meshIndex, int vertexIndex,
        const Vec3& localPosition);
    static bool EnsureReady(CEntity& entity);
};

#endif // __EDITOR_EDITOR_MESH_EDIT_H__
