#include "tex.h"
#include "engine/material_texture_restore.h"
#include "models.h"
#include "editor/editor_preferences.h"
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
namespace fs = std::filesystem;

bool CTextureMetadataStore::IsImageFile(const fs::path& path)
{
    std::string ext = path.extension().string();
    for (auto& c : ext)
    {
        c = (char)tolower(c);
    }
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

namespace
{

std::string MakeTextureGuid()
{
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<unsigned long long> distribution;
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << distribution(generator)
           << std::setw(16) << distribution(generator);
    return stream.str();
}

bool ParseMetaValue(const fs::path& texturePath, const char* pKey, std::string& value)
{
    std::ifstream file(texturePath.string() + ".meta");
    if (!file.is_open())
    {
        return false;
    }
    std::string line;
    const std::string prefix = std::string(pKey) + ":";
    while (std::getline(file, line))
    {
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.rfind(prefix, 0) != 0)
        {
            continue;
        }
        value = line.substr(prefix.size());
        value.erase(0, value.find_first_not_of(" \t"));
        if (value.empty())
        {
            continue;
        }
        return true;
    }
    return false;
}

} // anonymous

fs::path CTextureMetadataStore::PathFromMeta(const fs::path& path)
{
    if (path.extension() == ".meta" && path.stem().extension() != ".meta")
    {
        fs::path texture = path;
        texture.replace_extension("");
        return texture;
    }
    return path;
}

bool CTextureMetadataStore::Save(const fs::path& texturePath, const STextureMeta& meta)
{
    std::ofstream file(texturePath.string() + ".meta", std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }
    file << "quark_texture_meta: 1\n"
         << "asset_id: " << meta.Guid << "\n"
         << "image:\n"
         << "  color_space: " << (meta.SrgbTexture ? "srgb" : "linear") << "\n"
         << "  alpha_mode: " << (meta.AlphaIsTransparency ? "transparency" : "straight") << "\n"
         << "  readable: " << (meta.IsReadable ? "true" : "false") << "\n"
         << "sampling:\n"
         << "  mipmaps: " << (meta.EnableMipMap ? "true" : "false") << "\n"
         << "  filter: " << (meta.FilterMode == 1 ? "linear" : "nearest") << "\n"
         << "  wrap_u: " << (meta.WrapU == 0 ? "repeat" : "clamp") << "\n"
         << "  wrap_v: " << (meta.WrapV == 0 ? "repeat" : "clamp") << "\n"
         << "limits:\n"
         << "  max_size: " << meta.MaxTextureSize << "\n"
         << "  compression: " << meta.CompressionQuality << "\n"
         << "sprite:\n"
         << "  mode: " << meta.SpriteMode << "\n"
         << "  type: " << meta.TextureType << "\n";
    return file.good();
}

bool CTextureMetadataStore::Load(const fs::path& texturePath, STextureMeta& meta)
{
    if (!fs::exists(texturePath.string() + ".meta"))
    {
        return false;
    }
    std::string value;
    if (ParseMetaValue(texturePath, "asset_id", value))
    {
        meta.Guid = value;
    }
    if (meta.Guid.empty())
    {
        ParseMetaValue(texturePath, "guid", meta.Guid);
    }
    if (meta.Guid.empty())
    {
        meta.Guid = MakeTextureGuid();
    }
    if (ParseMetaValue(texturePath, "mipmaps", value))
    {
        meta.EnableMipMap = value == "true";
    }
    if (ParseMetaValue(texturePath, "color_space", value))
    {
        meta.SrgbTexture = value == "srgb";
    }
    if (ParseMetaValue(texturePath, "readable", value))
    {
        meta.IsReadable = value == "true";
    }
    if (ParseMetaValue(texturePath, "filter", value))
    {
        meta.FilterMode = value == "linear" ? 1 : 0;
    }
    if (ParseMetaValue(texturePath, "wrap_u", value))
    {
        meta.WrapU = value == "repeat" ? 0 : 1;
    }
    if (ParseMetaValue(texturePath, "wrap_v", value))
    {
        meta.WrapV = value == "repeat" ? 0 : 1;
    }
    if (ParseMetaValue(texturePath, "max_size", value))
    {
        meta.MaxTextureSize = std::atoi(value.c_str());
    }
    if (ParseMetaValue(texturePath, "compression", value))
    {
        meta.CompressionQuality = std::atoi(value.c_str());
    }
    if (ParseMetaValue(texturePath, "mode", value))
    {
        meta.SpriteMode = std::atoi(value.c_str());
    }
    if (ParseMetaValue(texturePath, "type", value))
    {
        meta.TextureType = std::atoi(value.c_str());
    }
    if (ParseMetaValue(texturePath, "alpha_mode", value))
    {
        meta.AlphaIsTransparency = value == "transparency";
    }
    return true;
}

bool CTextureMetadataStore::Ensure(const fs::path& texturePath)
{
    if (!CTextureMetadataStore::IsImageFile(texturePath))
    {
        return false;
    }
    STextureMeta meta;
    if (!Load(texturePath, meta))
    {
        meta.Guid = MakeTextureGuid();
        return Save(texturePath, meta);
    }
    return Save(texturePath, meta);
}

void CTextureMetadataStore::ApplyToTexture(Texture2D& texture, const STextureMeta& meta)
{
    if (texture.id == 0)
    {
        return;
    }
    SetTextureFilter(texture, meta.FilterMode == 1 ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, meta.WrapU == 0 ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP);
    if (meta.EnableMipMap)
    {
        GenTextureMipmaps(&texture);
    }
}

void CEntityTextureService::ApplyTextureRepeat(CEntity& entity)
{
    CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    CMaterialComponent* pMatComponent = entity.GetMaterialComponent();
    const CTransformComponent* pTransform = entity.GetTransformComponent();
    if (!pMeshComponent || !pTransform || !pMatComponent)
    {
        return;
    }

    for (int m = 0; m < pMeshComponent->m_Model.meshCount; m++)
    {
        Mesh &mesh = pMeshComponent->m_Model.meshes[m];
        if (!mesh.texcoords)
        {
            continue;
        }

        for (int i = 0; i < mesh.vertexCount; i++)
        {
            float u, v;

            if (pMatComponent->m_AutoUv)
            {
                Vec3 pos = {
                    mesh.vertices[i*3+0],
                    mesh.vertices[i*3+1],
                    mesh.vertices[i*3+2]
                };

                Vec3 normal = {
                    mesh.normals[i*3+0],
                    mesh.normals[i*3+1],
                    mesh.normals[i*3+2]
                };

                float ax = fabs(normal.x);
                float ay = fabs(normal.y);
                float az = fabs(normal.z);

                float sx = pTransform->m_Scale.x;
                float sy = pTransform->m_Scale.y;
                float sz = pTransform->m_Scale.z;

                if (ay > ax && ay > az)
                {
                    u = pos.x * sx;
                    v = pos.z * sz;
                }
                else if (ax > az)
                {
                    u = pos.z * sz;
                    v = pos.y * sy;
                }
                else
                {
                    u = pos.x * sx;
                    v = pos.y * sy;
                }

                u *= pMatComponent->m_UvScale.x;
                v *= pMatComponent->m_UvScale.y;
            }
            else
            {
                if (m >= pMatComponent->m_vOriginalTexcoords.size())
                {
                    continue;
                }
                auto& vBase = pMatComponent->m_vOriginalTexcoords[m];

                u = vBase[i*2+0] * pMatComponent->m_TextureRepeatU * pTransform->m_Scale.x;
                v = vBase[i*2+1] * pMatComponent->m_TextureRepeatV * pTransform->m_Scale.y;
            }

            mesh.texcoords[i*2+0] = u;
            mesh.texcoords[i*2+1] = v;
        }

        UpdateMeshBuffer(mesh, 2, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    }
}

void CEntityTextureService::StoreUV(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    CMeshComponent* pMeshComponent = pEntity->GetMeshComponent();
    CMaterialComponent* pMatComponent = pEntity->GetMaterialComponent();
    if (!pMeshComponent || !pMatComponent)
    {
        return;
    }
    pMatComponent->m_vOriginalTexcoords.clear();

    for (int m = 0; m < pMeshComponent->m_Model.meshCount; m++)
    {
        Mesh& mesh = pMeshComponent->m_Model.meshes[m];

        if (!mesh.texcoords)
        {
            pMatComponent->m_vOriginalTexcoords.push_back({});
            continue;
        }

        std::vector<float> vUv(mesh.vertexCount * 2);
        memcpy(vUv.data(), mesh.texcoords, vUv.size() * sizeof(float));

        pMatComponent->m_vOriginalTexcoords.push_back(vUv);
    }

    pMeshComponent->m_UvDirty = true;
    pMeshComponent->m_BoundsDirty = true;
}

void CEntityTextureService::MarkEntityUVDirty(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    if (pMesh)
    {
        pMesh->m_UvDirty = true;
    }
}

void CEntityTextureService::MarkEntityBoundsDirty(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    if (pMesh)
    {
        pMesh->m_BoundsDirty = true;
    }
}

void CEntityTextureService::StoreMaterialTextures(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    CMaterialComponent* pMat = pEntity->GetMaterialComponent();
    if (!pMesh || !pMat)
    {
        return;
    }
    pMat->m_vOriginalMaterialTextures.clear();
    pMat->m_vOriginalMaterialTextures.reserve(pMesh->m_Model.materialCount);

    for (int i = 0; i < pMesh->m_Model.materialCount; i++)
    {
        if (!pMesh->m_Model.materials[i].maps)
        {
            pMat->m_vOriginalMaterialTextures.push_back({0});
            continue;
        }

        pMat->m_vOriginalMaterialTextures.push_back(
            pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture);
    }
}

void CEntityTextureService::RestoreModelTextures(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    CMaterialComponent* pMat = pEntity->GetMaterialComponent();
    if (!pMesh || !pMat)
    {
        return;
    }
    quark::RestoreOriginalMaterialTextures(*pMesh, *pMat);
}

void CEntityTextureService::ClearMaterialTextures(CEntity* pEntity)
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
    for (int i = 0; i < pMesh->m_Model.materialCount; i++)
    {
        if (pMesh->m_Model.materials[i].maps)
        {
            pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
        }
    }
}
void CEntityTextureService::RefreshEntityRenderState(CEntity& entity)
{
    CMeshComponent* pMesh = entity.GetMeshComponent();
    CMaterialComponent* pMat = entity.GetMaterialComponent();
    if (!pMesh || !pMesh->m_UvDirty || !pMat)
    {
        return;
    }

    if (pMat->m_Texture.id != 0)
    {
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (pMesh->m_Model.materials[i].maps)
            {
                pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = pMat->m_Texture;
            }
        }
    }

    if (pMat->m_TextureStretch)
    {
        for (int m = 0; m < pMesh->m_Model.meshCount; m++)
        {
            Mesh& modelMesh = pMesh->m_Model.meshes[m];
            if (!modelMesh.texcoords || m >= pMat->m_vOriginalTexcoords.size())
            {
                continue;
            }

            memcpy(modelMesh.texcoords, pMat->m_vOriginalTexcoords[m].data(), modelMesh.vertexCount * 2 * sizeof(float));
            UpdateMeshBuffer(modelMesh, 2, modelMesh.texcoords, modelMesh.vertexCount * 2 * sizeof(float), 0);
        }
    }
    else
    {
        CEntityTextureService::ApplyTextureRepeat(entity);
    }

    pMesh->m_UvDirty = false;
}

void DrawCollisionDebug(CEntity& entity, const Mat4& worldTransform, const CPreferences& preferences)
{
    CCollisionComponent* pCollision = entity.GetCollisionComponent();
    CMeshComponent* pMeshComponent = entity.GetMeshComponent();

    if (!pCollision || !pCollision->m_Visualize || !preferences.m_ShowColliders)
    {
        return;
    }

    Color lineColor = GREEN;
    Color pointColor = LIME;

    PushMatrix();
    MultMatrix(worldTransform);
    Translate(pCollision->m_Center.x, pCollision->m_Center.y, pCollision->m_Center.z);

    switch (pCollision->m_ColliderType)
    {
        case COLLIDER_BOX:
        {
            DrawCubeWires({0,0,0},
                pCollision->m_Size.x,
                pCollision->m_Size.y,
                pCollision->m_Size.z,
                lineColor);
            break;
        }

        case COLLIDER_SPHERE:
        {
            DrawSphereWires({0,0,0},
                pCollision->m_Radius,
                16, 16,
                lineColor);
            break;
        }

        case COLLIDER_CAPSULE:
        {
            float r = pCollision->m_Radius;
            float h = pCollision->m_Height;

            float cylinderH = std::max(0.0f, h - r * 2.0f);

            DrawCylinderWires({0,0,0}, r, r, cylinderH, 16, lineColor);

            DrawSphereWires({0, cylinderH * 0.5f, 0}, r, 12, 12, lineColor);
            DrawSphereWires({0,-cylinderH * 0.5f, 0}, r, 12, 12, lineColor);
            break;
        }

        case COLLIDER_MESH:
        {
            if (!pMeshComponent)
            {
                break;
            }

            DrawModelWires(pMeshComponent->m_Model, {0,0,0}, 1.0f, lineColor);
            break;
        }
    }

    PopMatrix();
}

void CEntityTextureService::DrawEntityWithTexture(CEntity& entity, const Mat4& worldTransform, const CPreferences& preferences)
{
    RefreshEntityRenderState(entity);
    const CMeshComponent* pMesh = entity.GetMeshComponent();
    const CMaterialComponent* pMat = entity.GetMaterialComponent();
    if (!pMesh || !pMat)
    {
        return;
    }

    PushMatrix();
    MultMatrix(worldTransform);

    const bool editedMeshIsDoubleSided = CMeshOverrideService::Has(entity) || pMesh->m_MeshTrianglesDetached;
    if (editedMeshIsDoubleSided)
    {
        DisableBackfaceCulling();
    }

    DrawModel(pMesh->m_Model, {0,0,0}, 1.0f, pMat->m_Color);
    if (preferences.m_WireframeEnabled && pMat->m_OutlineColor.a > 0)
    {
        Color wireframeColor = {
            static_cast<unsigned char>(preferences.m_WireframeRed),
            static_cast<unsigned char>(preferences.m_WireframeGreen),
            static_cast<unsigned char>(preferences.m_WireframeBlue), 255
        };
        DrawModelWires(pMesh->m_Model, {0,0,0}, 1.0f, wireframeColor);
    }

    if (editedMeshIsDoubleSided)
    {
        EnableBackfaceCulling();
    }

    PopMatrix();
    DrawCollisionDebug(entity, worldTransform, preferences);
}

void CEntityTextureService::CloneModelMaterials(CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    if (!pMesh || pMesh->m_Model.materialCount <= 0)
    {
        return;
    }

    if (pMesh->m_OwnsMaterials)
    {
        if (pMesh->m_Model.materials)
        {
            free(pMesh->m_Model.materials);
        }
        if (pMesh->m_Model.meshMaterial)
        {
            free(pMesh->m_Model.meshMaterial);
        }
        pMesh->m_Model.materials    = nullptr;
        pMesh->m_Model.meshMaterial = nullptr;
        pMesh->m_OwnsMaterials     = false;
    }
    if (!pMesh->m_pAsset || !pMesh->m_pAsset->m_LoadedModel.materials)
    {
        return;
    }

    Material* pCloned = (Material*)malloc(pMesh->m_Model.materialCount * sizeof(Material));
    memcpy(pCloned, pMesh->m_pAsset->m_LoadedModel.materials, pMesh->m_Model.materialCount * sizeof(Material));
    pMesh->m_Model.materials = pCloned;

    pMesh->m_Model.meshMaterial = (int*)malloc(pMesh->m_Model.meshCount * sizeof(int));
    memcpy(pMesh->m_Model.meshMaterial, pMesh->m_pAsset->m_LoadedModel.meshMaterial, pMesh->m_Model.meshCount * sizeof(int));

    pMesh->m_OwnsMaterials = true;
}
