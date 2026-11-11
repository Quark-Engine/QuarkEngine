#ifndef __COMPONENT_H__
#define __COMPONENT_H__
#include "QuarkCore/QuarkCore.hpp"
#include "QuarkGUI/QuarkGUI.h"
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
namespace quark::script { class CScriptComponent; }
using CScriptComponent = ::quark::script::CScriptComponent;

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
    COMPONENT_CUSTOM,
    COMPONENT_CAMERA,
    COMPONENT_SCRIPT,
    COMPONENT_UI,
    COMPONENT_TEXT,
    COMPONENT_UI_RECT_TRANSFORM
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

class CCameraComponent : public IComponent
{
public:
    float m_FieldOfView = 60.0f;

    CCameraComponent()
        : IComponent(COMPONENT_CAMERA, "Camera")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_CAMERA;
    }
    std::string GetTypeName() const override
    {
        return "Camera";
    }

    void Serialize(nlohmann::json& json) const override
    {
        json["field_of_view"] = m_FieldOfView;
    }

    void Deserialize(const nlohmann::json& json) override
    {
        if (json.contains("field_of_view") && json["field_of_view"].is_number())
        {
            m_FieldOfView = json["field_of_view"].get<float>();
        }
    }
};

enum class EUiTextFontStyle : int
{
    NORMAL = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3
};

enum class EUiTextAlignment : int
{
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2
};

enum class EUiTextVerticalAlignment : int
{
    TOP = 0,
    MIDDLE = 1,
    BOTTOM = 2
};

enum class EUiTextOverflow : int
{
    WRAP = 0,
    TRUNCATE = 1
};

class CUiRectTransformComponent : public IComponent
{
public:
    Vec2 m_Position = { 32.0f, 32.0f };
    Vec2 m_Size = { 150.0f, 30.0f };

    CUiRectTransformComponent()
        : IComponent(COMPONENT_UI_RECT_TRANSFORM, "Rect Transform")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_UI_RECT_TRANSFORM;
    }

    std::string GetTypeName() const override
    {
        return "Rect Transform";
    }

    void Serialize(nlohmann::json& json) const override
    {
        json["position"] = { m_Position.x, m_Position.y };
        json["size"] = { m_Size.x, m_Size.y };
    }

    void Deserialize(const nlohmann::json& json) override
    {
        if (json.contains("position") && json["position"].is_array() && json["position"].size() >= 2)
        {
            m_Position = { json["position"][0].get<float>(), json["position"][1].get<float>() };
        }
        if (json.contains("size") && json["size"].is_array() && json["size"].size() >= 2)
        {
            m_Size = { json["size"][0].get<float>(), json["size"][1].get<float>() };
        }
    }
};

class CTextComponent : public IComponent
{
public:
    std::string m_Text = "Text";
    std::string m_Font = "Arial";
    EUiTextFontStyle m_FontStyle = EUiTextFontStyle::NORMAL;
    float m_FontSize = 16.0f;
    float m_LineSpacing = 1.0f;
    bool m_RichText = false;
    EUiTextAlignment m_HorizontalAlignment = EUiTextAlignment::CENTER;
    EUiTextVerticalAlignment m_VerticalAlignment = EUiTextVerticalAlignment::MIDDLE;
    EUiTextOverflow m_HorizontalOverflow = EUiTextOverflow::WRAP;
    EUiTextOverflow m_VerticalOverflow = EUiTextOverflow::TRUNCATE;
    bool m_BestFit = false;
    Color m_Color = { 235, 235, 235, 255 };
    std::string m_Material = "None (Material)";

    CTextComponent()
        : IComponent(COMPONENT_TEXT, "Text")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_TEXT;
    }

    std::string GetTypeName() const override
    {
        return "Text";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CUiComponent : public IComponent
{
public:
    EGuiWidgetType m_WidgetType = EGuiWidgetType::Button;
    std::string m_Placeholder = "InputField";
    std::vector<std::string> m_vItems = { "Combo" };
    int m_SelectedIndex = 0;
    bool m_Interactable = true;
    EGuiButtonTransition m_Transition = EGuiButtonTransition::COLOR_TINT;
    Color m_NormalColor = { 72, 72, 72, 255 };
    Color m_HighlightedColor = { 88, 88, 88, 255 };
    Color m_PressedColor = { 58, 58, 58, 255 };
    Color m_SelectedColor = { 88, 88, 88, 255 };
    Color m_DisabledColor = { 50, 50, 50, 255 };
    float m_ColorMultiplier = 1.0f;
    float m_FadeDuration = 0.1f;
    EGuiButtonNavigation m_Navigation = EGuiButtonNavigation::AUTOMATIC;
    bool m_ShowNavigationVisualization = false;

    SGuiWidgetDesc m_RuntimeDesc;

    CUiComponent()
    {
        m_Name = "Ui Button";
        m_Type = COMPONENT_UI;
    }

    EComponentType GetType() const override
    {
        return COMPONENT_UI;
    }

    std::string GetTypeName() const override
    {
        switch (m_WidgetType)
        {
        case EGuiWidgetType::Button:
            return "Ui Button";
        case EGuiWidgetType::InputField:
            return "Ui Inputfield";
        case EGuiWidgetType::Combo:
            return "Ui Combo";
        }
        return "Ui Button";
    }

    void SyncToRuntimeDesc(const CUiRectTransformComponent* pRectTransform,
                           const CTextComponent* pTextComponent)
    {
        m_RuntimeDesc.widgetId = reinterpret_cast<uint64_t>(this);
        m_RuntimeDesc.widgetType = m_WidgetType;
        if (pRectTransform)
        {
            m_RuntimeDesc.position = pRectTransform->m_Position;
            m_RuntimeDesc.size = pRectTransform->m_Size;
        }
        else
        {
            m_RuntimeDesc.position = { 40.0f, 40.0f };
            m_RuntimeDesc.size = { 160.0f, 30.0f };
        }
        if (pTextComponent)
        {
            m_RuntimeDesc.text = pTextComponent->m_Text;
            m_RuntimeDesc.textFontSize = pTextComponent->m_FontSize;
            m_RuntimeDesc.textLineSpacing = pTextComponent->m_LineSpacing;
            m_RuntimeDesc.textColor = pTextComponent->m_Color;
            m_RuntimeDesc.textHorizontalAlignment = static_cast<int>(pTextComponent->m_HorizontalAlignment);
            m_RuntimeDesc.textVerticalAlignment = static_cast<int>(pTextComponent->m_VerticalAlignment);
        }
        else
        {
            m_RuntimeDesc.text.clear();
            m_RuntimeDesc.textFontSize = 16.0f;
            m_RuntimeDesc.textLineSpacing = 1.0f;
            m_RuntimeDesc.textColor = { 235, 235, 235, 255 };
            m_RuntimeDesc.textHorizontalAlignment = 1;
            m_RuntimeDesc.textVerticalAlignment = 1;
        }
        m_RuntimeDesc.placeholder = m_Placeholder;
        m_RuntimeDesc.vItems = m_vItems;
        m_RuntimeDesc.selectedIndex = m_SelectedIndex;
        m_RuntimeDesc.buttonInteractable = m_Interactable;
        m_RuntimeDesc.buttonTransition = m_Transition;
        m_RuntimeDesc.buttonNormalColor = m_NormalColor;
        m_RuntimeDesc.buttonHighlightedColor = m_HighlightedColor;
        m_RuntimeDesc.buttonPressedColor = m_PressedColor;
        m_RuntimeDesc.buttonSelectedColor = m_SelectedColor;
        m_RuntimeDesc.buttonDisabledColor = m_DisabledColor;
        m_RuntimeDesc.buttonColorMultiplier = m_ColorMultiplier;
        m_RuntimeDesc.buttonFadeDuration = m_FadeDuration;
        m_RuntimeDesc.buttonNavigation = m_Navigation;
        m_RuntimeDesc.showNavigationVisualization = m_ShowNavigationVisualization;
    }

    void SyncFromRuntimeDesc(CTextComponent* pTextComponent)
    {
        if (pTextComponent)
        {
            pTextComponent->m_Text = m_RuntimeDesc.text;
        }
        m_SelectedIndex = m_RuntimeDesc.selectedIndex;
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
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

    void RemoveMeshAndMaterialForUi()
    {
        for (size_t i = m_vComponents.size(); i > 0; --i)
        {
            const auto& pComponent = m_vComponents[i - 1];
            if (pComponent &&
                (pComponent->GetType() == COMPONENT_MESH ||
                 pComponent->GetType() == COMPONENT_MATERIAL))
            {
                if (auto pMesh = std::dynamic_pointer_cast<CMeshComponent>(pComponent))
                {
                    pMesh->ReleaseOwnedResources();
                }
                RemoveComponent(i - 1);
            }
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

    CCameraComponent* GetCamera()
    {
        auto pComp = GetComponentOfType<CCameraComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    CUiComponent* GetUiComponent()
    {
        auto pComp = GetComponentOfType<CUiComponent>();
        return pComp ? pComp.get() : nullptr;
    }

    CScriptComponent* GetScript();

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
