#ifndef __EDITOR_EDITOR_GIZMO_H__
#define __EDITOR_EDITOR_GIZMO_H__

class CEditor;
class CFlyCamera;

class CGizmoController
{
public:
    static void Draw(CEditor& editor, CFlyCamera& camera);
    static bool IsCustomGizmoOver();
    static bool IsCustomGizmoUsing();
};

#endif // __EDITOR_EDITOR_GIZMO_H__
