#ifndef __EDITOR_VIEWERS_H__
#define __EDITOR_VIEWERS_H__

#include "editor.h"
#include "../models.h"
#include <filesystem>
#include <string>
#include <vector>

bool OpenModelViewerForAsset(CModelViewerState& state, const CModelAsset& asset);
void DrawModelViewerWindow(CModelViewerState& state);

bool OpenMaterialViewerForPath(CEditor& editor, CMaterialViewerState& state, const std::filesystem::path& materialPath);
void DrawMaterialViewerWindow(CEditor& editor, CMaterialViewerState& state, CEntity* pSelectedEntity = nullptr);

void LoadMaterialToEntity(CEntity* pEntity, const std::filesystem::path& mtlPath, int materialSlot = -1);
std::vector<std::string> GetAllMaterialsInProject();

#endif // __EDITOR_VIEWERS_H__
