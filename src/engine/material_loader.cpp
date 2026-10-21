#include "engine/material_loader.h"

#include "entity.h"
#include "tex.h"

#include <fstream>
#include <sstream>
void LoadMaterialToEntity(CEntity* pEntity, const std::filesystem::path& mtlPath, int materialSlot)
{
    if (!pEntity || !std::filesystem::exists(mtlPath))
    {
        return;
    }

    std::ifstream materialFile(mtlPath);
    if (!materialFile.is_open())
    {
        return;
    }

    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    CMaterialComponent* pMatComp = pEntity->GetMaterialComponent();
    if (!pMesh || !pMatComp)
    {
        return;
    }
    const auto appliesToSlot = [pMesh, materialSlot](int materialIndex)
    {
        return materialSlot < 0 || materialIndex == materialSlot;
    };

    Color albedo = WHITE;
    std::string textureName;
    std::string normalTextureName;
    std::string roughnessTextureName;
    std::string metallicTextureName;

    std::string line;
    while (std::getline(materialFile, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "Kd")
        {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (stream >> r >> g >> b)
            {
                albedo = {
                    static_cast<unsigned char>(r * 255),
                    static_cast<unsigned char>(g * 255),
                    static_cast<unsigned char>(b * 255),
                    255
                };
            }
        }
        else if (type == "map_Kd")
        {
            if (!(stream >> textureName))
            {
                textureName.clear();
            }
        }
        else if (type == "map_Bump" || type == "bump" || type == "norm")
        {
            if (!(stream >> normalTextureName))
            {
                normalTextureName.clear();
            }
        }
        else if (type == "map_Pr" || type == "map_roughness")
        {
            if (!(stream >> roughnessTextureName))
            {
                roughnessTextureName.clear();
            }
        }
        else if (type == "map_Pm" || type == "map_metallic")
        {
            if (!(stream >> metallicTextureName))
            {
                metallicTextureName.clear();
            }
        }
    }

    materialFile.close();

    pMatComp->m_Color = albedo;
    pMatComp->m_AlbedoTextureName.clear();
    pMatComp->m_NormalTextureName = normalTextureName;
    pMatComp->m_RoughnessTextureName = roughnessTextureName;
    pMatComp->m_MetallicTextureName = metallicTextureName;
    if (pMesh->m_Model.materials)
    {
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (!appliesToSlot(i))
            {
                continue;
            }
            pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = albedo;
        }
    }

    if (!textureName.empty())
    {
        std::filesystem::path texturePath = mtlPath.parent_path() / textureName;
        if (std::filesystem::exists(texturePath))
        {
            Texture2D tex = LoadTexture(texturePath.string().c_str());
            if (tex.id != 0)
            {
                pMatComp->m_Texture = tex;
                pMatComp->m_TextureName = textureName;
                pMatComp->m_TextureSource = TEXTURE_EXTERNAL;

                if (pMesh->m_Model.materials)
                {
                    for (int i = 0; i < pMesh->m_Model.materialCount; i++)
                    {
                        if (!appliesToSlot(i))
                        {
                            continue;
                        }
                        pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
                    }
                }
            }
        }
    }

    const auto loadMap = [&](const std::string& path, int mapType)
    {
        if (path.empty() || !pMesh->m_Model.materials)
        {
            return;
        }
        const std::filesystem::path texturePath = mtlPath.parent_path() / path;
        if (!std::filesystem::exists(texturePath))
        {
            return;
        }
        Texture2D texture = LoadTexture(texturePath.string().c_str());
        if (texture.id == 0)
        {
            return;
        }
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (!appliesToSlot(i))
            {
                continue;
            }
            if (pMesh->m_Model.materials[i].maps)
            {
                pMesh->m_Model.materials[i].maps[mapType].texture = texture;
            }
        }
    };

    loadMap(normalTextureName, MATERIAL_MAP_NORMAL);
    loadMap(roughnessTextureName, MATERIAL_MAP_ROUGHNESS);
    loadMap(metallicTextureName, MATERIAL_MAP_METALNESS);

    pMatComp->m_TextureName = mtlPath.string();
}

