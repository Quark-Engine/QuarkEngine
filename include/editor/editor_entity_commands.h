#ifndef __EDITOR_EDITOR_ENTITY_COMMANDS_H__
#define __EDITOR_EDITOR_ENTITY_COMMANDS_H__

#include "../scene.h"

#include <string>

class CEditor;

class CSceneEntityCommands
{
public:
    static CEntity CloneInstance(const CEntity& source, CScene& scene);

    static void Duplicate(CEditor& editor, CEntity* pEntity);
    static void Erase(CEditor& editor, int index);
    static void Delete(CEditor& editor, CEntity* pEntity);

private:
    static std::string MakeDuplicateName(const CScene& scene, const std::string& name);
};

#endif // __EDITOR_EDITOR_ENTITY_COMMANDS_H__
