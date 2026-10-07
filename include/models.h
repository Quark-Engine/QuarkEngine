#ifndef __MODELS_H__
#define __MODELS_H__
#include "entity.h"
#include "text_mesh.h"

#include <filesystem>

class CModelService
{
public:
    static void UpdateModel(CEntity* pEntity, const CFreetypeTextMesh& textMesh);
    static void RebuildMeshNormals(Mesh& mesh);
    static bool EnsureAssetLoaded(CModelAsset& asset);
    static bool LoadInstance(const CModelAsset& asset, Model& model);
    static bool IsModelFile(const std::filesystem::path& path);

private:
#if defined(__unix__) || defined(__APPLE__)
    class CModelLoadGuard;

    static CModelLoadGuard ms_LoadGuard;
#endif
};

class CMeshOverrideService
{
public:
    static void Clear(CEntity& entity);
    static bool Has(const CEntity& entity);
    static void CaptureFromModel(CEntity& entity);
    static bool Apply(CEntity& entity);
    static bool GetTriangleVertexIndices(const Mesh& mesh, int triangleIndex, int aOutIndices[3]);
    static bool DetachTriangles(CEntity& entity);
};

#endif // __MODELS_H__