#ifndef __COMPONENT_H__
#define __COMPONENT_H__
#include "QuarkCore/QuarkCore.hpp"
#include "lighting.h"
#include "nlohmann/json.hpp"
#include "editable_mesh.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <typeinfo>
#include <unordered_map>
#include <cstdlib>

class CEntity;
class CModelAsset;
class CComponentFactoryRegistry;

enum EObjectType
{
    OBJECT_CUBE = 0,
    OBJECT_SPHERE = 1,
    OBJECT_CONE = 2,
    OBJECT_CYLINDER = 3,
    OBJECT_HEMISPHERE = 4,
    OBJECT_TORUS = 5
};

enum ETextureSource
{
    TEXTURE_NONE,
    TEXTURE_EXTERNAL,
    TEXTURE_MODEL
};

enum EComponentType
{
    COMPONENT_TRANSFORM,
    COMPONENT_MESH,
    COMPONENT_MATERIAL,
    COMPONENT_LIGHT,
    COMPONENT_COLLISION,
    COMPONENT_CUSTOM
};

enum EColliderType
{
    COLLIDER_BOX,
    COLLIDER_SPHERE,
    COLLIDER_CAPSULE,
    COLLIDER_MESH
};

class IComponent
{
public:
    std::string m_Name;
    bool m_Enabled = true;
    EComponentType m_Type;

    IComponent() = default;
    IComponent(EComponentType typeVal, const std::string& nameVal)
        : m_Name(nameVal), m_Type(typeVal)
        {
        }

    virtual ~IComponent() = default;
    virtual void Serialize(nlohmann::json& json) const
    {
    }
    virtual void Deserialize(const nlohmann::json& json)
    {
    }
    virtual EComponentType GetType() const
    {
        return COMPONENT_CUSTOM;
    }
    virtual std::string GetTypeName() const
    {
        return "Custom";
    }
    virtual void OnEntityTransformChanged()
    {
    }
};

using FComponentFactory = std::function<std::shared_ptr<IComponent>()>;

class CTransformComponent : public IComponent
{
public:
    Vec3 m_Position = {0, 0, 0};
    Vec3 m_Rotation = {0, 0, 0};
    Vec3 m_Scale = {1, 1, 1};
    Mat4 m_LocalMatrixOverride = Mat4::identity();
    bool m_HasLocalMatrixOverride = false;

    CTransformComponent()
    {
        m_Name = "Transform";
        m_Type = COMPONENT_TRANSFORM;
    }

    EComponentType GetType() const override
    {
        return COMPONENT_TRANSFORM;
    }
    std::string GetTypeName() const override
    {
        return "Transform";
    }

    void SetLocalMatrixOverride(const Mat4& matrix)
    {
        m_LocalMatrixOverride = matrix;
        m_HasLocalMatrixOverride = true;
    }

    void ClearLocalMatrixOverride()
    {
        m_HasLocalMatrixOverride = false;
    }

    void Serialize(nlohmann::json& json) const override
    {
        json["position"] = {m_Position.x, m_Position.y, m_Position.z};
        json["rotation"] = {m_Rotation.x, m_Rotation.y, m_Rotation.z};
        json["scale"] = {m_Scale.x, m_Scale.y, m_Scale.z};
        if (m_HasLocalMatrixOverride)
        {
            json["local_matrix_override"] = m_LocalMatrixOverride.m;
        }
    }

    void Deserialize(const nlohmann::json& json) override
    {
        if (json.contains("position"))
        {
            auto& p = json["position"];
            m_Position = {p[0], p[1], p[2]};
        }
        if (json.contains("rotation"))
        {
            auto& r = json["rotation"];
            m_Rotation = {r[0], r[1], r[2]};
        }
        if (json.contains("scale"))
        {
            auto& s = json["scale"];
            m_Scale = {s[0], s[1], s[2]};
        }
        if (json.contains("local_matrix_override") && json["local_matrix_override"].is_array() &&
            json["local_matrix_override"].size() == 16)
        {
            for (size_t index = 0; index < 16; ++index)
            {
                m_LocalMatrixOverride.m[index] = json["local_matrix_override"][index].get<float>();
            }
            m_HasLocalMatrixOverride = true;
        }
        else
        {
            ClearLocalMatrixOverride();
        }
    }
};

class CMeshComponent : public IComponent
{
public:
    Model m_Model;
    bool m_OwnsModelInstance = false;
    CModelAsset* m_pAsset = nullptr;
    std::string m_AssetName;
    bool m_MeshTrianglesDetached = false;
    std::vector<std::vector<float>> m_vMeshVertexOverrides;

    int m_Segments = 16;
    EObjectType m_Type = OBJECT_CUBE;

    bool m_ShaderAssigned = false;
    bool m_OwnsMaterials = false;
    bool m_UvDirty = true;
    bool m_BoundsDirty = true;
    BoundingBox m_CachedLocalBounds = {{0, 0, 0}, {0, 0, 0}};

    CEditableMesh m_EditableMesh;
    bool m_IsEditableMesh = false;
    bool m_VertexGizmo = false;

    CMeshComponent()
    {
        m_Name = "Mesh";
    }

    CMeshComponent(const CMeshComponent& other)
        : IComponent(other),
          m_Model{},
          m_OwnsModelInstance(false),
          m_pAsset(other.m_pAsset),
          m_AssetName(other.m_AssetName),
          m_MeshTrianglesDetached(false),
          m_vMeshVertexOverrides(other.m_vMeshVertexOverrides),
          m_Segments(other.m_Segments),
          m_Type(other.m_Type),
          m_ShaderAssigned(false),
          m_OwnsMaterials(false),
          m_UvDirty(other.m_UvDirty),
          m_BoundsDirty(other.m_BoundsDirty),
          m_CachedLocalBounds(other.m_CachedLocalBounds),
          m_EditableMesh(other.m_EditableMesh),
          m_IsEditableMesh(other.m_IsEditableMesh),
          m_VertexGizmo(other.m_VertexGizmo)
    {
    }

    CMeshComponent& operator=(const CMeshComponent& other)
    {
        if (this == &other) return *this;

        IComponent::operator=(other);
        m_Model = {};
        m_OwnsModelInstance = false;
        m_pAsset = other.m_pAsset;
        m_AssetName = other.m_AssetName;
        m_MeshTrianglesDetached = false;
        m_vMeshVertexOverrides = other.m_vMeshVertexOverrides;
        m_Segments = other.m_Segments;
        m_Type = other.m_Type;
        m_ShaderAssigned = false;
        m_OwnsMaterials = false;
        m_UvDirty = other.m_UvDirty;
        m_BoundsDirty = other.m_BoundsDirty;
        m_CachedLocalBounds = other.m_CachedLocalBounds;
        m_EditableMesh = other.m_EditableMesh;
        m_IsEditableMesh = other.m_IsEditableMesh;
        m_VertexGizmo = other.m_VertexGizmo;
        return *this;
    }

    EComponentType GetType() const override
    {
        return COMPONENT_MESH;
    }
    std::string GetTypeName() const override
    {
        return "Mesh";
    }

    void ReleaseOwnedResources()
    {
        ClearRuntimeShadowMapBindings();

        if (m_OwnsMaterials)
        {
            if (m_Model.materials)
            {
                free(m_Model.materials);
                m_Model.materials = nullptr;
            }

            if (m_Model.meshMaterial)
            {
                free(m_Model.meshMaterial);
                m_Model.meshMaterial = nullptr;
            }

            m_Model.materialCount = 0;
            m_OwnsMaterials = false;
        }

        if (m_OwnsModelInstance && m_Model.meshCount > 0 && m_Model.meshes)
        {
            UnloadModel(m_Model);
        }

        m_Model = {};
        m_OwnsModelInstance = false;
    }

    void ClearRuntimeShadowMapBindings()
    {
        if (!m_Model.materials)
        {
            return;
        }

        for (int materialIndex = 0; materialIndex < m_Model.materialCount; ++materialIndex)
        {
            Material& material = m_Model.materials[materialIndex];
            if (!material.maps)
            {
                continue;
            }

            for (int shadowIndex = 0; shadowIndex < QC_MAX_LIGHTS; ++shadowIndex)
            {
                material.maps[MATERIAL_MAP_HEIGHT + shadowIndex].texture = {};
            }
        }
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CMaterialComponent : public IComponent
{
public:
    Texture2D m_Texture = {0};
    ETextureSource m_TextureSource = TEXTURE_NONE;
    std::string m_AlbedoTextureName;
    std::string m_TextureName;
    std::string m_NormalTextureName;
    std::string m_RoughnessTextureName;
    std::string m_MetallicTextureName;
    std::vector<std::string> m_vMaterialSlotSources;

    Color m_Color = WHITE;
    Color m_OutlineColor = LIGHTGRAY;

    bool m_AutoUv = false;
    bool m_TextureStretch = true;
    float m_TextureRepeatU = 1.0f;
    float m_TextureRepeatV = 1.0f;

    Vec2 m_UvScale = {1, 1};
    std::vector<std::vector<float>> m_vOriginalTexcoords;
    std::vector<Texture2D> m_vOriginalMaterialTextures;

    CMaterialComponent()
    {
        m_Name = "Material";
        m_Type = COMPONENT_MATERIAL;
    }

    EComponentType GetType() const override
    {
        return COMPONENT_MATERIAL;
    };
    std::string GetTypeName() const override
    {
        return "Material";
    };

    void Serialize(nlohmann::json& json) const override
    {
        json["color"] = { m_Color.r, m_Color.g, m_Color.b, m_Color.a };
        json["outline_color"] = { m_OutlineColor.r, m_OutlineColor.g, m_OutlineColor.b, m_OutlineColor.a };

        json["texture_source"] = static_cast<int>(m_TextureSource);
        json["albedo_texture_name"] = m_AlbedoTextureName;
        json["texture_name"] = m_TextureName;
        json["normal_texture_name"] = m_NormalTextureName;
        json["roughness_texture_name"] = m_RoughnessTextureName;
        json["metallic_texture_name"] = m_MetallicTextureName;
        json["material_slot_sources"] = m_vMaterialSlotSources;
        json["texture_stretch"] = m_TextureStretch;

        json["auto_uv"] = m_AutoUv;
        json["repeat_u"] = m_TextureRepeatU;
        json["repeat_v"] = m_TextureRepeatV;
        json["uv_scale"] = { m_UvScale.x, m_UvScale.y };
    }

    void Deserialize(const nlohmann::json& json) override
    {
        if (json.contains("color"))
        {
            auto& c = json["color"];
            m_Color = { c[0], c[1], c[2], c[3] };
        }

        if (json.contains("outline_color"))
        {
            auto& c = json["outline_color"];
            m_OutlineColor = { c[0], c[1], c[2], c[3] };
        }

        if (json.contains("texture_source")) m_TextureSource = static_cast<ETextureSource>(json["texture_source"].get<int>());
        if (json.contains("albedo_texture_name")) m_AlbedoTextureName = json["albedo_texture_name"];
        if (json.contains("texture_name")) m_TextureName = json["texture_name"];
        if (json.contains("normal_texture_name")) m_NormalTextureName = json["normal_texture_name"];
        if (json.contains("roughness_texture_name")) m_RoughnessTextureName = json["roughness_texture_name"];
        if (json.contains("metallic_texture_name")) m_MetallicTextureName = json["metallic_texture_name"];
        if (json.contains("material_slot_sources")) m_vMaterialSlotSources = json["material_slot_sources"].get<std::vector<std::string>>();
        if (json.contains("texture_stretch")) m_TextureStretch = json["texture_stretch"];

        if (json.contains("auto_uv")) m_AutoUv = json["auto_uv"];
        if (json.contains("repeat_u")) m_TextureRepeatU = json["repeat_u"];
        if (json.contains("repeat_v")) m_TextureRepeatV = json["repeat_v"];
        if (json.contains("uv_scale"))
        {
            auto& scale = json["uv_scale"];
            m_UvScale = { scale[0], scale[1] };
        }
    }
};

class CLightComponent : public IComponent
{
public:
    bool m_Created = false;
    CLightState m_Light;
    CLightComponent();

    EComponentType GetType() const override
    {
        return COMPONENT_LIGHT;
    }
    std::string GetTypeName() const override
    {
        return "Light";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
    void OnEntityTransformChanged() override;
};

class CCollisionComponent : public IComponent
{
public:
    EColliderType m_ColliderType = COLLIDER_BOX;

    bool m_IsTrigger = false;
    bool m_Visualize = true;

    // box
    Vec3 m_Size = {1, 1, 1};

    // sphere/capsule
    float m_Radius = 0.5f;
    float m_Height = 2.0f;

    Vec3 m_Center = {0, 0, 0};

    BoundingBox m_WorldBounds = {{0, 0, 0}, {0, 0, 0}};
    bool m_Dirty = true;

    CCollisionComponent()
    {
        m_Name = "Collision";
        m_Type = COMPONENT_COLLISION;
    }

    EComponentType GetType() const override
    {
        return COMPONENT_COLLISION;
    }
    std::string GetTypeName() const override
    {
        return "Collision";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
    void OnEntityTransformChanged() override;
};

class CComponentManager
{
private:
    std::vector<std::shared_ptr<IComponent>> m_vComponents;

public:
    void AddComponent(std::shared_ptr<IComponent> pComponent)
    {
        m_vComponents.push_back(pComponent);
    }

    void RemoveComponent(size_t index)
    {
        if (index < m_vComponents.size())
        {
            m_vComponents.erase(m_vComponents.begin() + index);
        }
    }

    std::shared_ptr<IComponent> GetComponent(size_t index)
    {
        if (index < m_vComponents.size())
        {
            return m_vComponents[index];
        }
        return nullptr;
    }

    size_t GetComponentCount() const
    {
        return m_vComponents.size();
    }

    template<typename T>
    std::shared_ptr<T> GetComponentOfType()
    {
        for (auto& comp : m_vComponents)
        {
            auto casted = std::dynamic_pointer_cast<T>(comp);
            if (casted) return casted;
        }
        return nullptr;
    }

    CTransformComponent* GetTransform()
    {
        auto pComp = GetComponentOfType<CTransformComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    CMeshComponent* GetMesh()
    {
        auto pComp = GetComponentOfType<CMeshComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    CLightComponent* GetLight()
    {
        auto pComp = GetComponentOfType<CLightComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    CMaterialComponent* GetMaterial()
    {
        auto pComp = GetComponentOfType<CMaterialComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    CCollisionComponent* GetCollision()
    {
        auto pComp = GetComponentOfType<CCollisionComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    const std::vector<std::shared_ptr<IComponent>>& GetAllComponents() const
    {
        return m_vComponents;
    }

    std::vector<std::shared_ptr<IComponent>>& GetAllComponents()
    {
        return m_vComponents;
    }

    void Serialize(nlohmann::json& json) const
    {
        json["components"] = nlohmann::json::array();
        for (const auto& comp : m_vComponents)
        {
            nlohmann::json compJson;
            compJson["type"] = comp->GetTypeName();
            compJson["enabled"] = comp->m_Enabled;
            nlohmann::json data;
            comp->Serialize(data);
            compJson["data"] = data;
            json["components"].push_back(compJson);
        }
    }

    void Deserialize(const nlohmann::json& json, const CComponentFactoryRegistry& factories);

    void CloneInto(const CComponentManager& source);
};

class CText3DComponent : public IComponent
{
public:
    std::string m_Text = "3D Text";
    float m_Size = 1.0f;
    float m_Thickness = 0.2f;
    float m_LetterSpacing = 0.1f;
    std::string m_FontPath;

    CText3DComponent()
    {
        m_Name = "3D Text"; m_Type = COMPONENT_CUSTOM;
    }

    std::string GetTypeName() const override
    {
        return "3D Text";
    }
    EComponentType GetType() const override
    {
        return COMPONENT_CUSTOM;
    }

    void Serialize(nlohmann::json& j) const override
    {
        j["text"] = m_Text;
        j["size"] = m_Size;
        j["thickness"] = m_Thickness;
        j["letter_spacing"] = m_LetterSpacing;
        j["font_path"] = m_FontPath;
    }

    void Deserialize(const nlohmann::json& j) override
    {
        if (j.contains("text")) m_Text = j["text"];
        if (j.contains("size")) m_Size = j["size"];
        if (j.contains("thickness")) m_Thickness = j["thickness"];
        if (j.contains("letter_spacing")) m_LetterSpacing = j["letter_spacing"];
        if (j.contains("font_path")) m_FontPath = j["font_path"];
    }
};

#endif // __COMPONENT_H__
