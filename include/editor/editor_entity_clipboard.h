#ifndef __EDITOR_EDITOR_ENTITY_CLIPBOARD_H__
#define __EDITOR_EDITOR_ENTITY_CLIPBOARD_H__

class CEditor;
class CEntity;

class CEntityClipboard
{
public:
    static void Copy(CEditor& editor, CEntity* pEntity);
    static void Paste(CEditor& editor);
};

#endif // __EDITOR_EDITOR_ENTITY_CLIPBOARD_H__
