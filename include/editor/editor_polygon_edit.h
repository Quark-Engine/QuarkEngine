#ifndef __EDITOR_EDITOR_POLYGON_EDIT_H__
#define __EDITOR_EDITOR_POLYGON_EDIT_H__

#include "QuarkCore/QuarkCore.hpp"
#include "../scene.h"

class CEditor;

class CPolygonEditor
{
public:
    static void Draw(CEditor& editor, const Camera3D& camera);

    static bool CreateVertex(const CScene& scene, CEntity& entity, const Vec3& worldPosition);
    static void CreateTriangle(CEntity& entity, int a, int b, int c);
};

#endif // __EDITOR_EDITOR_POLYGON_EDIT_H__
