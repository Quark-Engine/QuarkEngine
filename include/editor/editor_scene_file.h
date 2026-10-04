#ifndef __EDITOR_EDITOR_SCENE_FILE_H__
#define __EDITOR_EDITOR_SCENE_FILE_H__

#include <string>

class CEditor;

class CSceneFileService
{
public:
    static void SaveAs(CEditor& editor);

private:
    static std::string BrowseSceneSavePath(const std::string& defaultName);
};

#endif // __EDITOR_EDITOR_SCENE_FILE_H__