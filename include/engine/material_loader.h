#ifndef __ENGINE_MATERIAL_LOADER_H__
#define __ENGINE_MATERIAL_LOADER_H__

#include <filesystem>

class CEntity;

void LoadMaterialToEntity(CEntity* pEntity, const std::filesystem::path& materialPath,
                          int materialSlot = -1);

#endif // __ENGINE_MATERIAL_LOADER_H__
