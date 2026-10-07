#ifndef __EDITOR_ASSETS_H__
#define __EDITOR_ASSETS_H__

#include "../editor.h"
#include "../models.h"
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

struct SLocalEntry
{
    std::string FileName;
    bool IsDirectory;
    bool IsImage;
    bool IsModel;
    bool IsMaterial;
    bool IsTextureMeta;
    bool isPrefab;
    struct Texture Texture;
    std::string Extension;
};


void DrawAssetsUi(CEditor& editor);
void DrawSelectedTextureInspector(CEditor& editor);

bool ImportPathToResources(const fs::path& src, const fs::path& resourceDir);

void CleanupAssetsUi(CEditor& editor);

#endif // __EDITOR_ASSETS_H__
