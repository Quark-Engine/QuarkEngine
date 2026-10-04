#include "editor/editor_utils.h"
#include "QuarkCore/QuarkCore.hpp"
#include "entity.h"
#include "models.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace qc;

namespace fs = std::filesystem;

bool HasValidModelData(const Model& model)
{
    return model.meshCount > 0 && model.meshes != nullptr;
}

Vec3 GetSceneDropPosition(Camera3D camera)
{
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    const float epsilon = 0.0001f;

    if (fabsf(ray.direction.y) > epsilon)
    {
        const float t = -ray.position.y / ray.direction.y;
        if (t >= 0.0f)
        {
            return {
                ray.position.x + ray.direction.x * t,
                0.0f,
                ray.position.z + ray.direction.z * t
            };
        }
    }

    return {
        camera.position.x + camera.target.x,
        0.0f,
        camera.position.z + camera.target.z
    };
}

void ApplyNegativeScaleWinding(CEntity* pEntity)
{
    if (!pEntity) return;

    CTransformComponent* pTransform = pEntity->GetTransformComponent();
    CMeshComponent* pMeshComp = pEntity->GetMeshComponent();
    if (!pTransform || !pMeshComp) return;

    float sx = pTransform->m_Scale.x;
    float sy = pTransform->m_Scale.y;
    float sz = pTransform->m_Scale.z;

    int negCount = (sx < 0.0f ? 1 : 0) + (sy < 0.0f ? 1 : 0) + (sz < 0.0f ? 1 : 0);
    if (negCount % 2 == 0) return;

    for (int m = 0; m < pMeshComp->m_Model.meshCount; m++)
    {
        Mesh& mesh = pMeshComp->m_Model.meshes[m];
        if (!mesh.vertices || mesh.triangleCount <= 0) continue;

        if (mesh.indices)
        {
            for (int t = 0; t < mesh.triangleCount; t++)
            {
                std::swap(mesh.indices[t * 3 + 1], mesh.indices[t * 3 + 2]);
            }

            UpdateMeshBuffer(mesh, 6, mesh.indices, mesh.triangleCount * 3 * sizeof(unsigned short), 0);
        }
        else
        {
            for (int t = 0; t < mesh.triangleCount; t++)
            {
                int b = t * 3;
                for (int c = 0; c < 3; c++)
                {
                    std::swap(mesh.vertices[(b + 1) * 3 + c], mesh.vertices[(b + 2) * 3 + c]);
                }

                if (mesh.texcoords)
                {
                    for (int c = 0; c < 2; c++)
                    {
                        std::swap(mesh.texcoords[(b + 1) * 2 + c], mesh.texcoords[(b + 2) * 2 + c]);
                    }
                }
            }
            UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);
            if (mesh.texcoords)
                UpdateMeshBuffer(mesh, 2, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
        }

        CModelService::RebuildMeshNormals(mesh);
        UpdateMeshBuffer(mesh, 1, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);
    }
}
