#ifndef __EDITOR_EDITOR_SCENE_DROP_H__
#define __EDITOR_EDITOR_SCENE_DROP_H__

#include "QuarkCore/QuarkCore.hpp"

class CEditor;

class CSceneAssetDrop
{
public:
    static void Handle(CEditor& editor, const Camera3D& camera);
};

#endif // __EDITOR_EDITOR_SCENE_DROP_H__