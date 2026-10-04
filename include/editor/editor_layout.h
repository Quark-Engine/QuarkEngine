#ifndef __EDITOR_EDITOR_LAYOUT_H__
#define __EDITOR_EDITOR_LAYOUT_H__

#include "imgui.h"

class CEditor;
class CPreferences;

class CEditorLayout
{
public:
    static void Reset(ImGuiID dockspaceId, CPreferences& preferences);
    static void EnsureInitialized(CEditor& editor, ImGuiID dockspaceId);
};

#endif // __EDITOR_EDITOR_LAYOUT_H__