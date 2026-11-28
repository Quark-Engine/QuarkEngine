#ifndef __EDITOR_EDITOR_SCENE_DROP_H__
#define __EDITOR_EDITOR_SCENE_DROP_H__

#include "QuarkCore/QuarkCore.hpp"

#include <filesystem>
#include <string>

class CEditor;
class CEntity;

class CSceneAssetDrop
{
public:
    static void Handle(CEditor& editor, const Camera3D& camera);

    static bool IsTextureExtension(const std::filesystem::path& path);
    static bool IsMaterialExtension(const std::filesystem::path& path);
    static bool IsScriptExtension(const std::filesystem::path& path);
    static bool IsModelPath(const CEditor& editor, const std::string& assetName, const std::filesystem::path& path);

    static bool ApplyTextureToEntity(CEditor& editor, CEntity& entity,
                                     const std::filesystem::path& texturePath, bool asNormalMap = false);
    static bool ApplyMaterialToEntity(CEditor& editor, CEntity& entity,
                                      const std::filesystem::path& materialPath, int slot = -1);
    static bool ApplyScriptToEntity(CEditor& editor, CEntity& entity,
                                    const std::filesystem::path& scriptPath);
    static bool ApplyModelToEntity(CEditor& editor, CEntity& entity,
                                   const std::string& assetName, const std::filesystem::path& assetPath);
    static bool ApplyAssetToEntity(CEditor& editor, CEntity& entity,
                                   const std::string& assetName, const std::string& assetPath);
};

#endif // __EDITOR_EDITOR_SCENE_DROP_H__
