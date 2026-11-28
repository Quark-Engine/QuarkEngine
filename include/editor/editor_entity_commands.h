#ifndef __EDITOR_EDITOR_ENTITY_COMMANDS_H__
#define __EDITOR_EDITOR_ENTITY_COMMANDS_H__

#include "../scene.h"

#include <string>
#include <vector>

class CEditor;

class CSceneEntityCommands
{
public:
    static CEntity CloneInstance(const CEntity& source, CScene& scene);
    static std::string MakeDuplicateName(const CScene& scene, const std::string& name);
    static std::vector<int> GetRootSelectedIndices(const CScene& scene);

    static void Duplicate(CEditor& editor, CEntity* pEntity);
    static void DuplicateSelection(CEditor& editor);
    static void Erase(CEditor& editor, int index);
    static void EraseSelection(CEditor& editor);
    static void Delete(CEditor& editor, CEntity* pEntity);
    static void DeleteSelection(CEditor& editor);
    static int GroupSelection(CEditor& editor);
};

#endif // __EDITOR_EDITOR_ENTITY_COMMANDS_H__
