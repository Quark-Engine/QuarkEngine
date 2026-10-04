#include "models.h"
#include "scene.h"
#include "text_mesh.h"
#include "editor/editor_utils.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <csetjmp>
#include <cstdlib>
#endif

using namespace qc;

namespace
{

std::string LowercaseCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

Vec3 SafeNormalize(Vec3 value)
{
    const float length = sqrtf(value.x*value.x + value.y*value.y + value.z*value.z);
    if (length <= 0.000001f)
    {
        return { 0.0f, 1.0f, 0.0f };
    }
    return { value.x / length, value.y / length, value.z / length };
}

bool DetachSingleMeshTriangles(Mesh& mesh)
{
    if (!mesh.vertices || mesh.vertexCount <= 0 || mesh.triangleCount <= 0)
    {
        return false;
    }
    if (!mesh.indices)
    {
        return true;
    }

    const int detachedVertexCount = mesh.triangleCount * 3;
    std::vector<float> vVertices(detachedVertexCount * 3);
    std::vector<float> vTexcoords;
    std::vector<float> vNormals;
    std::vector<unsigned char> vColors;

    if (mesh.texcoords)
    {
        vTexcoords.resize(detachedVertexCount * 2);
    }
    if (mesh.normals)
    {
        vNormals.resize(detachedVertexCount * 3);
    }
    if (mesh.colors)
    {
        vColors.resize(detachedVertexCount * 4);
    }

    for (int triangleIndex = 0; triangleIndex < mesh.triangleCount; triangleIndex++)
    {
        int aSourceIndices[3] = {};
        if (!CMeshOverrideService::GetTriangleVertexIndices(mesh, triangleIndex, aSourceIndices))
        {
            return false;
        }

        for (int corner = 0; corner < 3; corner++)
        {
            const int srcIndex = aSourceIndices[corner];
            const int dstIndex = triangleIndex * 3 + corner;

            vVertices[dstIndex * 3 + 0] = mesh.vertices[srcIndex * 3 + 0];
            vVertices[dstIndex * 3 + 1] = mesh.vertices[srcIndex * 3 + 1];
            vVertices[dstIndex * 3 + 2] = mesh.vertices[srcIndex * 3 + 2];

            if (mesh.texcoords)
            {
                vTexcoords[dstIndex * 2 + 0] = mesh.texcoords[srcIndex * 2 + 0];
                vTexcoords[dstIndex * 2 + 1] = mesh.texcoords[srcIndex * 2 + 1];
            }

            if (mesh.normals)
            {
                vNormals[dstIndex * 3 + 0] = mesh.normals[srcIndex * 3 + 0];
                vNormals[dstIndex * 3 + 1] = mesh.normals[srcIndex * 3 + 1];
                vNormals[dstIndex * 3 + 2] = mesh.normals[srcIndex * 3 + 2];
            }

            if (mesh.colors)
            {
                vColors[dstIndex * 4 + 0] = mesh.colors[srcIndex * 4 + 0];
                vColors[dstIndex * 4 + 1] = mesh.colors[srcIndex * 4 + 1];
                vColors[dstIndex * 4 + 2] = mesh.colors[srcIndex * 4 + 2];
                vColors[dstIndex * 4 + 3] = mesh.colors[srcIndex * 4 + 3];
            }
        }
    }

    Mesh detached = mesh;
    detached.vertexCount = detachedVertexCount;
    detached.vertices = new float[vVertices.size()];
    detached.texcoords = mesh.texcoords ? new float[vTexcoords.size()] : nullptr;
    detached.normals = mesh.normals ? new float[vNormals.size()] : nullptr;
    detached.colors = mesh.colors ? new unsigned char[vColors.size()] : nullptr;
    detached.indices = nullptr;
    detached.animVertices = nullptr;
    detached.animNormals = nullptr;
    detached.boneWeights = nullptr;
    detached.boneCount = 0;
    detached.vaoId = 0;
    detached.vboId = 0;

    if (!detached.vertices ||
        (mesh.texcoords && !detached.texcoords) ||
        (mesh.normals && !detached.normals) ||
        (mesh.colors && !detached.colors))
    {
        delete[] detached.vertices;
        delete[] detached.texcoords;
        delete[] detached.normals;
        delete[] detached.colors;
        return false;
    }

    memcpy(detached.vertices, vVertices.data(), vVertices.size() * sizeof(float));
    if (detached.texcoords)
    {
        memcpy(detached.texcoords, vTexcoords.data(), vTexcoords.size() * sizeof(float));
    }
    if (detached.normals)
    {
        memcpy(detached.normals, vNormals.data(), vNormals.size() * sizeof(float));
    }
    if (detached.colors)
    {
        memcpy(detached.colors, vColors.data(), vColors.size() * sizeof(unsigned char));
    }

    CModelService::RebuildMeshNormals(detached);
    UploadMesh(&detached, false);
    UnloadMesh(mesh);
    mesh = detached;
    return true;
}

} // anonymous

#if defined(__unix__) || defined(__APPLE__)
class CModelService::CModelLoadGuard
{
public:
    void Install();

    void Remove();

private:
    static void HandleSignal(int signalCode);

    sigjmp_buf m_JumpBuffer;
    volatile sig_atomic_t m_IsArmed = 0;
    struct sigaction m_PreviousSegv =
    {
    };
    struct sigaction m_PreviousBus =
    {
    };
    struct sigaction m_PreviousAbt =
    {
    };
};

CModelService::CModelLoadGuard CModelService::ms_LoadGuard;

void CModelService::CModelLoadGuard::HandleSignal(int signalCode)
{
    if (ms_LoadGuard.m_IsArmed)
    {
        siglongjmp(ms_LoadGuard.m_JumpBuffer, signalCode);
    }
    std::_Exit(128 + signalCode);
}

void CModelService::CModelLoadGuard::Install()
{
    struct sigaction action =
    {
    };
    sigemptyset(&action.sa_mask);
    action.sa_handler = HandleSignal;
    action.sa_flags = 0;

    sigaction(SIGSEGV, &action, &ms_LoadGuard.m_PreviousSegv);
#ifdef SIGBUS
    sigaction(SIGBUS, &action, &ms_LoadGuard.m_PreviousBus);
#endif
    sigaction(SIGABRT, &action, &ms_LoadGuard.m_PreviousAbt);
    ms_LoadGuard.m_IsArmed = 1;
}

void CModelService::CModelLoadGuard::Remove()
{
    ms_LoadGuard.m_IsArmed = 0;
    sigaction(SIGSEGV, &ms_LoadGuard.m_PreviousSegv, nullptr);
#ifdef SIGBUS
    sigaction(SIGBUS, &ms_LoadGuard.m_PreviousBus, nullptr);
#endif
    sigaction(SIGABRT, &ms_LoadGuard.m_PreviousAbt, nullptr);
}
#endif

bool CMeshOverrideService::GetTriangleVertexIndices(const qc::Mesh& mesh, int triangleIndex, int aOutIndices[3])
{
    if (!aOutIndices || triangleIndex < 0 || triangleIndex >= mesh.triangleCount)
    {
        return false;
    }

    if (mesh.indices)
    {
        const int base = triangleIndex * 3;
        aOutIndices[0] = static_cast<int>(mesh.indices[base + 0]);
        aOutIndices[1] = static_cast<int>(mesh.indices[base + 1]);
        aOutIndices[2] = static_cast<int>(mesh.indices[base + 2]);
    }
    else
    {
        const int base = triangleIndex * 3;
        aOutIndices[0] = base + 0;
        aOutIndices[1] = base + 1;
        aOutIndices[2] = base + 2;
    }

    for (int i = 0; i < 3; i++)
    {
        if (aOutIndices[i] < 0 || aOutIndices[i] >= mesh.vertexCount)
        {
            return false;
        }
    }

    return true;
}

void CModelService::RebuildMeshNormals(qc::Mesh& mesh)
{
    if (!mesh.vertices || !mesh.normals || mesh.vertexCount <= 0)
    {
        return;
    }

    for (int i = 0; i < mesh.vertexCount * 3; i++)
    {
        mesh.normals[i] = 0.0f;
    }

    for (int triangleIndex = 0; triangleIndex < mesh.triangleCount; triangleIndex++)
    {
        int aIndices[3] = {};
        if (!CMeshOverrideService::GetTriangleVertexIndices(mesh, triangleIndex, aIndices))
        {
            continue;
        }

        const Vec3 a = {
            mesh.vertices[aIndices[0] * 3 + 0],
            mesh.vertices[aIndices[0] * 3 + 1],
            mesh.vertices[aIndices[0] * 3 + 2]
        };
        const Vec3 b = {
            mesh.vertices[aIndices[1] * 3 + 0],
            mesh.vertices[aIndices[1] * 3 + 1],
            mesh.vertices[aIndices[1] * 3 + 2]
        };
        const Vec3 c = {
            mesh.vertices[aIndices[2] * 3 + 0],
            mesh.vertices[aIndices[2] * 3 + 1],
            mesh.vertices[aIndices[2] * 3 + 2]
        };

        const Vec3 ab = b - a;
        const Vec3 ac = c - a;
        const Vec3 normal = SafeNormalize(ab.cross(ac));

        for (int i = 0; i < 3; i++)
        {
            mesh.normals[aIndices[i] * 3 + 0] += normal.x;
            mesh.normals[aIndices[i] * 3 + 1] += normal.y;
            mesh.normals[aIndices[i] * 3 + 2] += normal.z;
        }
    }

    for (int vertexIndex = 0; vertexIndex < mesh.vertexCount; vertexIndex++)
    {
        const Vec3 accumulated = {
            mesh.normals[vertexIndex * 3 + 0],
            mesh.normals[vertexIndex * 3 + 1],
            mesh.normals[vertexIndex * 3 + 2]
        };
        const Vec3 normalized = SafeNormalize(accumulated);
        mesh.normals[vertexIndex * 3 + 0] = normalized.x;
        mesh.normals[vertexIndex * 3 + 1] = normalized.y;
        mesh.normals[vertexIndex * 3 + 2] = normalized.z;
    }
}

void CMeshOverrideService::Clear(CEntity& entity)
{
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return;
    }
    pMesh->m_vMeshVertexOverrides.clear();
    pMesh->m_MeshTrianglesDetached = false;
}

bool CMeshOverrideService::Has(const CEntity& entity)
{
    const CMeshComponent* pMesh = entity.GetMeshComponent();
    return pMesh && !pMesh->m_vMeshVertexOverrides.empty();
}

void CMeshOverrideService::CaptureFromModel(CEntity& entity)
{
    CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    if (!pMeshComponent)
    {
        return;
    }

    const bool trianglesDetached = pMeshComponent->m_MeshTrianglesDetached;
    pMeshComponent->m_vMeshVertexOverrides.clear();
    pMeshComponent->m_MeshTrianglesDetached = trianglesDetached;
    if (pMeshComponent->m_Model.meshCount <= 0 || !pMeshComponent->m_Model.meshes)
    {
        return;
    }

    pMeshComponent->m_vMeshVertexOverrides.reserve(pMeshComponent->m_Model.meshCount);
    for (int meshIndex = 0; meshIndex < pMeshComponent->m_Model.meshCount; meshIndex++)
    {
        const Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
        if (!mesh.vertices || mesh.vertexCount <= 0)
        {
            pMeshComponent->m_vMeshVertexOverrides.emplace_back();
            continue;
        }

        const float* pBegin = mesh.vertices;
        const float* pEnd = mesh.vertices + mesh.vertexCount * 3;
        pMeshComponent->m_vMeshVertexOverrides.emplace_back(pBegin, pEnd);
    }
}

bool CMeshOverrideService::Apply(CEntity& entity)
{
    CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    if (!pMeshComponent)
    {
        return false;
    }
    if (!Has(entity))
    {
        return false;
    }
    if (pMeshComponent->m_Model.meshCount <= 0 || !pMeshComponent->m_Model.meshes)
    {
        return false;
    }

    if (pMeshComponent->m_MeshTrianglesDetached)
    {
        DetachTriangles(entity);
    }

    bool appliedAny = false;

    for (int meshIndex = 0; meshIndex < pMeshComponent->m_Model.meshCount; meshIndex++)
    {
        if (meshIndex >= static_cast<int>(pMeshComponent->m_vMeshVertexOverrides.size()))
        {
            break;
        }

        Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
        std::vector<float>& vOverrideVertices = pMeshComponent->m_vMeshVertexOverrides[meshIndex];
        if (!mesh.vertices || mesh.vertexCount <= 0)
        {
            continue;
        }
        if (vOverrideVertices.size() != static_cast<size_t>(mesh.vertexCount * 3))
        {
            continue;
        }

        memcpy(mesh.vertices, vOverrideVertices.data(), vOverrideVertices.size() * sizeof(float));
        UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);

        if (mesh.normals)
        {
            CModelService::RebuildMeshNormals(mesh);
            UpdateMeshBuffer(mesh, 1, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);
        }

        appliedAny = true;
    }

    return appliedAny;
}


bool CMeshOverrideService::DetachTriangles(CEntity& entity)
{
    CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    if (!pMeshComponent || pMeshComponent->m_Model.meshCount <= 0 || !pMeshComponent->m_Model.meshes)
    {
        return false;
    }

    bool detachedAny = false;
    for (int meshIndex = 0; meshIndex < pMeshComponent->m_Model.meshCount; meshIndex++)
    {
        Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
        if (!mesh.indices)
        {
            continue;
        }
        if (!DetachSingleMeshTriangles(mesh))
        {
            continue;
        }
        detachedAny = true;
    }

    if (detachedAny)
    {
        pMeshComponent->m_MeshTrianglesDetached = true;
    }
    else if (!pMeshComponent->m_MeshTrianglesDetached)
    {
        pMeshComponent->m_MeshTrianglesDetached = true;
    }

    return detachedAny;
}

bool CModelService::IsModelFile(const std::filesystem::path& path)
{
    std::string ext = LowercaseCopy(path.extension().string());

    return ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".iqm";
}

bool CModelService::EnsureAssetLoaded(CModelAsset& asset)
{
    if (asset.m_IsProcedural)
    {
        return static_cast<bool>(asset.pfnGenerator);
    }

    if (asset.m_LoadedModel.meshCount > 0 && asset.m_LoadedModel.meshes)
    {
        return true;
    }

    if (asset.m_FilePath.empty())
    {
        return false;
    }

    asset.m_LoadedModel = LoadModel(asset.m_FilePath.c_str());
    return asset.m_LoadedModel.meshCount > 0 && asset.m_LoadedModel.meshes != nullptr;
}

bool CModelService::LoadInstance(const CModelAsset& asset, qc::Model& model)
{
    if (asset.m_IsProcedural)
    {
        if (!asset.pfnGenerator)
        {
            model = {};
            return false;
        }

        model = asset.pfnGenerator(32);
        return model.meshCount > 0 && model.meshes != nullptr;
    }

    if (asset.m_FilePath.empty())
    {
        model = {};
        return false;
    }

    model = LoadModel(asset.m_FilePath.c_str());
    return model.meshCount > 0 && model.meshes != nullptr;
}

void CModelService::UpdateModel(CEntity* pEntity, const CFreetypeTextMesh& textMesh)
{
    if (!pEntity)
    {
        return;
    }

    CMeshComponent* pMesh = pEntity->GetMeshComponent();

    if (!pMesh)
    {
        return;
    }

    pMesh->ReleaseOwnedResources();

    if (auto* pText3D = pEntity->GetComponents()->GetComponentOfType<CText3DComponent>().get())
    {
        std::string fontPath = pText3D->m_FontPath.empty() ? CFreetypeTextMesh::GetDefaultFontPath() : pText3D->m_FontPath;
        pMesh->m_Model = textMesh.Generate(pText3D->m_Text, pText3D->m_Size, pText3D->m_Thickness, pText3D->m_LetterSpacing, fontPath);
        pMesh->m_OwnsModelInstance = true;
        return;
    }

    if (pMesh->m_IsEditableMesh)
    {
        pMesh->m_Model = {};

        RebuildMeshFromEditable(
            pMesh->m_Model,
            pMesh->m_EditableMesh
        );

        pMesh->m_OwnsModelInstance = true;
        ApplyNegativeScaleWinding(pEntity);

        return;
    }

    if (!pMesh->m_pAsset || !pMesh->m_pAsset->m_IsProcedural || !pMesh->m_pAsset->pfnGenerator)
    {
        return;
    }


    int maxSeg = 125;

    if (pMesh->m_Type == OBJECT_SPHERE || pMesh->m_Type == OBJECT_HEMISPHERE)
    {
        maxSeg = 100;
    }


    if (pMesh->m_Segments < 3)
    {
        pMesh->m_Segments = 3;
    }

    if (pMesh->m_Segments > maxSeg)
    {
        pMesh->m_Segments = maxSeg;
    }

    pMesh->m_Model = pMesh->m_pAsset->pfnGenerator(pMesh->m_Segments);
    pMesh->m_OwnsModelInstance = true;

    ApplyNegativeScaleWinding(pEntity);
}
