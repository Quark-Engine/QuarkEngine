#ifndef __COMPONENT_H__
#define __COMPONENT_H__

#include "QuarkCore/QuarkCore.hpp"
#include "QuarkGUI/QuarkGUI.h"
#include "lighting.h"
#include "nlohmann/json.hpp"
#include "editable_mesh.h"
#include <array>
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
namespace quark::script
{
    class CScriptComponent;
} // quark::script
using CScriptComponent = ::quark::script::CScriptComponent;

enum EObjectType
{
    OBJECT_CUBE = 0,
    OBJECT_SPHERE = 1,
    OBJECT_CONE = 2,
    OBJECT_CYLINDER = 3,
    OBJECT_HEMISPHERE = 4,
    OBJECT_TORUS = 5,
    OBJECT_CAPSULE = 6
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
    COMPONENT_RIGIDBODY,
    COMPONENT_CUSTOM,
    COMPONENT_CAMERA,
    COMPONENT_SCRIPT,
    COMPONENT_UI,
    COMPONENT_TEXT,
    COMPONENT_UI_RECT_TRANSFORM,
    COMPONENT_CANVAS,
    COMPONENT_IMAGE,
    COMPONENT_PANEL,
    COMPONENT_BUTTON,
    COMPONENT_SLIDER,
    COMPONENT_TOGGLE,
    COMPONENT_PROGRESS_BAR,
    COMPONENT_SCROLL_VIEW,
    COMPONENT_HORIZONTAL_LAYOUT,
    COMPONENT_VERTICAL_LAYOUT,
    COMPONENT_GRID_LAYOUT,
    COMPONENT_COUNT
};

enum class ECanvasRenderMode
{
    SCREEN_SPACE_OVERLAY = 0,
    SCREEN_SPACE_CAMERA = 1,
    WORLD_SPACE = 2
};

enum class EUiCanvasViewport
{
    EDITOR_SCENE = 0,
    EDITOR_GAME = 1,
    RUNTIME = 2
};

enum class EUiImageType
{
    SIMPLE = 0,
    SLICED = 1,
    TILED = 2,
    FILLED = 3
};

enum class EUiTransition
{
    NONE = 0,
    COLOR_TINT = 1,
    SPRITE_SWAP = 2,
    ANIMATION = 3
};

enum class EUiNavigation
{
    NONE = 0,
    HORIZONTAL = 1,
    VERTICAL = 2,
    AUTOMATIC = 3,
    EXPLICIT = 4
};

enum class EUiSliderDirection : int
{
    LEFT_TO_RIGHT = 0,
    RIGHT_TO_LEFT = 1,
    BOTTOM_TO_TOP = 2,
    TOP_TO_BOTTOM = 3
};

enum class EUiLayoutChildAlignment : int
{
    UPPER_LEFT = 0,
    UPPER_CENTER = 1,
    UPPER_RIGHT = 2,
    MIDDLE_LEFT = 3,
    MIDDLE_CENTER = 4,
    MIDDLE_RIGHT = 5,
    LOWER_LEFT = 6,
    LOWER_CENTER = 7,
    LOWER_RIGHT = 8
};

enum class EUiGridStartCorner : int
{
    UPPER_LEFT = 0,
    UPPER_RIGHT = 1,
    LOWER_LEFT = 2,
    LOWER_RIGHT = 3
};

enum class EUiGridStartAxis : int
{
    HORIZONTAL = 0,
    VERTICAL = 1
};

enum class EUiGridConstraint : int
{
    FLEXIBLE = 0,
    FIXED_COLUMN_COUNT = 1,
    FIXED_ROW_COUNT = 2
};

struct SUiLayoutPadding
{
    float m_Left = 0.0f;
    float m_Right = 0.0f;
    float m_Top = 0.0f;
    float m_Bottom = 0.0f;
};

enum class EUiEventType
{
    POINTER_ENTER = 0,
    POINTER_EXIT = 1,
    POINTER_DOWN = 2,
    POINTER_UP = 3,
    CLICK = 4,
    BEGIN_DRAG = 5,
    DRAG = 6,
    END_DRAG = 7,
    DROP = 8,
    SELECT = 9,
    DESELECT = 10,
    SUBMIT = 11
};

struct SUiEventCallback
{
    EUiEventType m_Event = EUiEventType::CLICK;
    std::string m_Action;
};

enum EColliderType
{
    COLLIDER_BOX,
    COLLIDER_SPHERE,
    COLLIDER_CAPSULE,
    COLLIDER_MESH
};

enum class ECapsuleDirection : int
{
    X_AXIS = 0,
    Y_AXIS = 1,
    Z_AXIS = 2
};

enum class EPhysicMaterialPreset : int
{
    NONE = 0,
    DEFAULT = 1,
    BOUNCY = 2,
    ICE = 3,
    RUBBER = 4,
    METAL = 5
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
    Vec3 m_Position = { 0, 0, 0 };
    Vec3 m_Rotation = { 0, 0, 0 };
    Vec3 m_Scale = { 1, 1, 1 };
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
        json["position"] = { m_Position.x, m_Position.y, m_Position.z };
        json["rotation"] = { m_Rotation.x, m_Rotation.y, m_Rotation.z };
        json["scale"] = { m_Scale.x, m_Scale.y, m_Scale.z };
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
            m_Position = { p[0], p[1], p[2] };
        }
        if (json.contains("rotation"))
        {
            auto& r = json["rotation"];
            m_Rotation = { r[0], r[1], r[2] };
        }
        if (json.contains("scale"))
        {
            auto& s = json["scale"];
            m_Scale = { s[0], s[1], s[2] };
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
    BoundingBox m_CachedLocalBounds = { { 0, 0, 0 }, { 0, 0, 0 } };

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
        if (this == &other)
        {
            return *this;
        }

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
    Texture2D m_Texture = { 0 };
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

    Vec2 m_UvScale = { 1, 1 };
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

        if (json.contains("texture_source"))
        {
            m_TextureSource = static_cast<ETextureSource>(json["texture_source"].get<int>());
        }
        if (json.contains("albedo_texture_name"))
        {
            m_AlbedoTextureName = json["albedo_texture_name"];
        }
        if (json.contains("texture_name"))
        {
            m_TextureName = json["texture_name"];
        }
        if (json.contains("normal_texture_name"))
        {
            m_NormalTextureName = json["normal_texture_name"];
        }
        if (json.contains("roughness_texture_name"))
        {
            m_RoughnessTextureName = json["roughness_texture_name"];
        }
        if (json.contains("metallic_texture_name"))
        {
            m_MetallicTextureName = json["metallic_texture_name"];
        }
        if (json.contains("material_slot_sources"))
        {
            m_vMaterialSlotSources = json["material_slot_sources"].get<std::vector<std::string>>();
        }
        if (json.contains("texture_stretch"))
        {
            m_TextureStretch = json["texture_stretch"];
        }

        if (json.contains("auto_uv"))
        {
            m_AutoUv = json["auto_uv"];
        }
        if (json.contains("repeat_u"))
        {
            m_TextureRepeatU = json["repeat_u"];
        }
        if (json.contains("repeat_v"))
        {
            m_TextureRepeatV = json["repeat_v"];
        }
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
    bool m_ProvidesContacts = true;
    bool m_Visualize = true;
    EPhysicMaterialPreset m_MaterialPreset = EPhysicMaterialPreset::NONE;

    Vec3 m_Size = { 1, 1, 1 };

    float m_Radius = 0.5f;
    float m_Height = 2.0f;
    ECapsuleDirection m_Direction = ECapsuleDirection::Y_AXIS;

    bool m_Convex = false;

    Vec3 m_Center = { 0, 0, 0 };

    BoundingBox m_WorldBounds = { { 0, 0, 0 }, { 0, 0, 0 } };
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

    float GetEffectiveFriction(float fallback) const
    {
        switch (m_MaterialPreset)
        {
            case EPhysicMaterialPreset::DEFAULT:
                return 0.5f;
            case EPhysicMaterialPreset::BOUNCY:
                return 0.2f;
            case EPhysicMaterialPreset::ICE:
                return 0.04f;
            case EPhysicMaterialPreset::RUBBER:
                return 0.9f;
            case EPhysicMaterialPreset::METAL:
                return 0.35f;
            case EPhysicMaterialPreset::NONE:
            default:
                return fallback;
        }
    }

    float GetEffectiveRestitution(float fallback) const
    {
        switch (m_MaterialPreset)
        {
            case EPhysicMaterialPreset::DEFAULT:
                return 0.1f;
            case EPhysicMaterialPreset::BOUNCY:
                return 0.85f;
            case EPhysicMaterialPreset::ICE:
                return 0.05f;
            case EPhysicMaterialPreset::RUBBER:
                return 0.65f;
            case EPhysicMaterialPreset::METAL:
                return 0.25f;
            case EPhysicMaterialPreset::NONE:
            default:
                return fallback;
        }
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
    void OnEntityTransformChanged() override;
};

enum class ERigidbodyInterpolate : int
{
    NONE = 0,
    INTERPOLATE = 1,
    EXTRAPOLATE = 2
};

enum class ECollisionDetectionMode : int
{
    DISCRETE = 0,
    CONTINUOUS = 1,
    CONTINUOUS_DYNAMIC = 2,
    CONTINUOUS_SPECULATIVE = 3
};

class CRigidbodyComponent : public IComponent
{
public:
    float m_Mass = 1.0f;
    float m_LinearDrag = 0.05f;
    float m_AngularDrag = 0.05f;
    float m_Friction = 0.5f;
    float m_Restitution = 0.2f;
    bool m_UseGravity = true;
    float m_GravityScale = 1.0f;
    bool m_IsKinematic = false;
    ERigidbodyInterpolate m_Interpolate = ERigidbodyInterpolate::NONE;
    ECollisionDetectionMode m_CollisionDetection = ECollisionDetectionMode::DISCRETE;
    bool m_FreezePositionX = false;
    bool m_FreezePositionY = false;
    bool m_FreezePositionZ = false;
    bool m_FreezeRotationX = false;
    bool m_FreezeRotationY = false;
    bool m_FreezeRotationZ = false;

    Vec3 m_Velocity = { 0.0f, 0.0f, 0.0f };
    Vec3 m_AngularVelocity = { 0.0f, 0.0f, 0.0f };
    Vec3 m_AccumulatedForce = { 0.0f, 0.0f, 0.0f };
    Vec3 m_AccumulatedTorque = { 0.0f, 0.0f, 0.0f };
    bool m_IsGrounded = false;
    bool m_IsSleeping = false;
    float m_SleepTimer = 0.0f;

    Vec3 m_PrevPosition = { 0.0f, 0.0f, 0.0f };
    Vec3 m_PrevRotation = { 0.0f, 0.0f, 0.0f };
    Vec3 m_PhysicsPosition = { 0.0f, 0.0f, 0.0f };
    Vec3 m_PhysicsRotation = { 0.0f, 0.0f, 0.0f };
    Vec3 m_LastRenderedPosition = { 0.0f, 0.0f, 0.0f };
    Vec3 m_LastRenderedRotation = { 0.0f, 0.0f, 0.0f };
    bool m_HasInterpolationState = false;

    CRigidbodyComponent()
    {
        m_Name = "Rigidbody";
        m_Type = COMPONENT_RIGIDBODY;
    }

    EComponentType GetType() const override
    {
        return COMPONENT_RIGIDBODY;
    }
    std::string GetTypeName() const override
    {
        return "Rigidbody";
    }

    float GetInverseMass() const
    {
        if (!m_Enabled || m_IsKinematic)
        {
            return 0.0f;
        }
        return 1.0f / (m_Mass > 0.001f ? m_Mass : 0.001f);
    }

    void WakeUp()
    {
        m_IsSleeping = false;
        m_SleepTimer = 0.0f;
    }

    void AddForce(const Vec3& force)
    {
        if (!m_Enabled || m_IsKinematic)
        {
            return;
        }
        WakeUp();
        m_AccumulatedForce = m_AccumulatedForce + force;
    }

    void AddImpulse(const Vec3& impulse)
    {
        if (!m_Enabled || m_IsKinematic)
        {
            return;
        }
        WakeUp();
        const float invMass = GetInverseMass();
        m_Velocity = m_Velocity + impulse * invMass;
    }

    void SetVelocity(const Vec3& velocity)
    {
        WakeUp();
        m_Velocity = velocity;
    }

    void AddTorque(const Vec3& torque)
    {
        if (!m_Enabled || m_IsKinematic)
        {
            return;
        }
        WakeUp();
        m_AccumulatedTorque = m_AccumulatedTorque + torque;
    }

    void SetAngularVelocity(const Vec3& angularVelocity)
    {
        WakeUp();
        m_AngularVelocity = angularVelocity;
    }

    void ResetRuntimeState()
    {
        m_Velocity = { 0.0f, 0.0f, 0.0f };
        m_AngularVelocity = { 0.0f, 0.0f, 0.0f };
        m_AccumulatedForce = { 0.0f, 0.0f, 0.0f };
        m_AccumulatedTorque = { 0.0f, 0.0f, 0.0f };
        m_IsGrounded = false;
        m_IsSleeping = false;
        m_SleepTimer = 0.0f;
        m_HasInterpolationState = false;
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
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

class CCanvasComponent : public IComponent
{
public:
    ECanvasRenderMode m_RenderMode = ECanvasRenderMode::WORLD_SPACE;
    int m_RenderCameraEntityId = -1;
    bool m_PixelPerfect = false;
    int m_SortingOrder = 0;
    int m_TargetDisplay = 0;
    Vec2 m_ReferenceResolution = { 1920.0f, 1080.0f };
    float m_MatchWidthOrHeight = 0.5f;
    float m_Width = 800.0f;
    float m_Height = 600.0f;
    float m_ScaleFactor = 1.0f;
    float m_PlaneDistance = 100.0f;
    int m_EventCameraEntityId = -1;
    bool m_OverrideSorting = false;

    RenderTexture2D m_RuntimeTexture = {};
    std::array<RenderTexture2D, 3> m_aScreenSpaceTextures = {};
    Model m_RuntimeQuadModel = {};
    bool m_QuadModelInitialized = false;
    bool m_CanvasDirty = true;
    std::uint64_t m_LastRenderHash = 0;
    std::array<std::uint64_t, 3> m_aScreenSpaceHashes = {};
    RendererType m_QuadBackend = RendererType::Auto;

    CCanvasComponent()
        : IComponent(COMPONENT_CANVAS, "Canvas")
    {
    }

    ~CCanvasComponent() override;

    EComponentType GetType() const override
    {
        return COMPONENT_CANVAS;
    }

    std::string GetTypeName() const override
    {
        return "Canvas";
    }

    void ReleaseOwnedResources();
    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CUiRectTransformComponent : public IComponent
{
public:
    Vec2 m_Position = { 32.0f, 32.0f };
    Vec2 m_AnchoredPosition = { 32.0f, 32.0f };
    Vec2 m_Size = { 150.0f, 30.0f };
    Vec2 m_AnchorMin = { 0.5f, 0.5f };
    Vec2 m_AnchorMax = { 0.5f, 0.5f };
    Vec2 m_Pivot = { 0.5f, 0.5f };
    float m_Rotation2D = 0.0f;
    Vec2 m_Scale2D = { 1.0f, 1.0f };
    Vec2 m_OffsetMin = { 0.0f, 0.0f };
    Vec2 m_OffsetMax = { 0.0f, 0.0f };
    bool m_ClipChildren = false;

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

    Rectangle CalculateLocalRect(const Vec2& parentSize) const;
    void SetAnchoredPosition(const Vec2& anchoredPosition);

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CImageComponent : public IComponent
{
public:
    std::string m_SourceTexture;
    Color m_Color = { 255, 255, 255, 255 };
    std::string m_Material;
    bool m_RaycastTarget = true;
    EUiImageType m_ImageType = EUiImageType::SIMPLE;
    bool m_PreserveAspect = false;
    bool m_FillCenter = true;
    float m_FillAmount = 1.0f;
    std::array<float, 4> m_aBorder = {};

    Texture2D m_RuntimeTexture = {};
    bool m_TextureLoaded = false;
    std::string m_LoadedTexturePath;

    CImageComponent()
        : IComponent(COMPONENT_IMAGE, "Image")
    {
    }

    ~CImageComponent() override;

    EComponentType GetType() const override
    {
        return COMPONENT_IMAGE;
    }

    std::string GetTypeName() const override
    {
        return "Image";
    }

    void ReleaseOwnedResources();
    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CPanelComponent : public IComponent
{
public:
    std::string m_BackgroundImage;
    Color m_BackgroundColor = { 255, 255, 255, 100 };
    bool m_RaycastTarget = true;

    Texture2D m_RuntimeTexture = {};
    bool m_TextureLoaded = false;
    std::string m_LoadedTexturePath;

    CPanelComponent()
        : IComponent(COMPONENT_PANEL, "Panel")
    {
    }

    ~CPanelComponent() override;

    EComponentType GetType() const override
    {
        return COMPONENT_PANEL;
    }

    std::string GetTypeName() const override
    {
        return "Panel";
    }

    void ReleaseOwnedResources();
    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CButtonComponent : public IComponent
{
public:
    bool m_Interactable = true;
    EUiTransition m_Transition = EUiTransition::COLOR_TINT;
    EUiNavigation m_Navigation = EUiNavigation::AUTOMATIC;
    Color m_NormalColor = { 255, 255, 255, 255 };
    Color m_HighlightedColor = { 220, 220, 220, 255 };
    Color m_PressedColor = { 180, 180, 180, 255 };
    Color m_SelectedColor = { 220, 220, 220, 255 };
    Color m_DisabledColor = { 150, 150, 150, 128 };
    std::array<int, 4> m_aNavigationTargetIds = { -1, -1, -1, -1 };
    float m_ColorMultiplier = 1.0f;
    float m_FadeDuration = 0.1f;
    std::string m_SpriteSwapHighlighted;
    std::string m_SpriteSwapPressed;
    std::string m_SpriteSwapSelected;
    std::string m_SpriteSwapDisabled;
    std::string m_OnClickAction;
    std::vector<SUiEventCallback> m_vEventCallbacks;
    std::function<void()> m_fnOnClick;
    bool m_RaycastTarget = true;

    bool m_IsHovered = false;
    bool m_IsPressed = false;
    bool m_IsSelected = false;

    CButtonComponent()
        : IComponent(COMPONENT_BUTTON, "Button")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_BUTTON;
    }

    std::string GetTypeName() const override
    {
        return "Button";
    }

    Color GetCurrentColor() const;
    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CTextComponent : public IComponent
{
public:
    std::string m_Text = "Text";
    std::string m_Font = "Arial";
    EUiTextFontStyle m_FontStyle = EUiTextFontStyle::NORMAL;
    float m_FontSize = 16.0f;
    float m_LineSpacing = 1.0f;
    float m_CharacterSpacing = 0.0f;
    bool m_RichText = false;
    EUiTextAlignment m_HorizontalAlignment = EUiTextAlignment::CENTER;
    EUiTextVerticalAlignment m_VerticalAlignment = EUiTextVerticalAlignment::MIDDLE;
    EUiTextOverflow m_HorizontalOverflow = EUiTextOverflow::WRAP;
    EUiTextOverflow m_VerticalOverflow = EUiTextOverflow::TRUNCATE;
    bool m_BestFit = false;
    Color m_Color = { 235, 235, 235, 255 };
    std::string m_Material = "None (Material)";
    bool m_RaycastTarget = true;

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

class CSliderComponent : public IComponent
{
public:
    bool m_Interactable = true;
    EUiTransition m_Transition = EUiTransition::COLOR_TINT;
    Color m_BackgroundColor = { 45, 45, 52, 255 };
    Color m_FillColor = { 70, 145, 240, 255 };
    Color m_HandleColor = { 235, 235, 240, 255 };
    Color m_HighlightedColor = { 255, 255, 255, 255 };
    Color m_PressedColor = { 180, 180, 195, 255 };
    Color m_DisabledColor = { 90, 90, 95, 160 };
    EUiSliderDirection m_Direction = EUiSliderDirection::LEFT_TO_RIGHT;
    float m_MinValue = 0.0f;
    float m_MaxValue = 1.0f;
    bool m_WholeNumbers = false;
    float m_Value = 0.5f;
    std::string m_OnValueChangedAction;
    bool m_RaycastTarget = true;

    bool m_IsHovered = false;
    bool m_IsPressed = false;

    CSliderComponent()
        : IComponent(COMPONENT_SLIDER, "Slider")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_SLIDER;
    }

    std::string GetTypeName() const override
    {
        return "Slider";
    }

    float GetNormalizedValue() const;
    void SetNormalizedValue(float normalized);
    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CToggleComponent : public IComponent
{
public:
    bool m_Interactable = true;
    EUiTransition m_Transition = EUiTransition::COLOR_TINT;
    bool m_IsOn = true;
    Color m_BoxColor = { 55, 55, 65, 255 };
    Color m_HighlightedColor = { 75, 75, 90, 255 };
    Color m_PressedColor = { 40, 40, 50, 255 };
    Color m_DisabledColor = { 40, 40, 45, 150 };
    Color m_CheckmarkColor = { 80, 180, 255, 255 };
    Color m_LabelColor = { 235, 238, 245, 255 };
    int m_FontSize = 18;
    std::string m_Label = "Toggle";
    std::string m_OnValueChangedAction;
    bool m_RaycastTarget = true;

    bool m_IsHovered = false;
    bool m_IsPressed = false;

    CToggleComponent()
        : IComponent(COMPONENT_TOGGLE, "Toggle")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_TOGGLE;
    }

    std::string GetTypeName() const override
    {
        return "Toggle";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CProgressBarComponent : public IComponent
{
public:
    float m_MinValue = 0.0f;
    float m_MaxValue = 1.0f;
    float m_Value = 0.65f;
    EUiSliderDirection m_Direction = EUiSliderDirection::LEFT_TO_RIGHT;
    Color m_BackgroundColor = { 38, 40, 48, 255 };
    Color m_FillColor = { 76, 175, 80, 255 };
    Color m_BorderColor = { 90, 95, 110, 255 };
    Color m_TextColor = { 240, 242, 248, 255 };
    bool m_ShowPercentageText = true;
    bool m_RaycastTarget = false;

    CProgressBarComponent()
        : IComponent(COMPONENT_PROGRESS_BAR, "Progress Bar")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_PROGRESS_BAR;
    }

    std::string GetTypeName() const override
    {
        return "Progress Bar";
    }

    float GetNormalizedValue() const;
    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CScrollViewComponent : public IComponent
{
public:
    int m_ContentEntityId = -1;
    bool m_Horizontal = false;
    bool m_Vertical = true;
    int m_MovementType = 1;
    float m_Elasticity = 0.1f;
    bool m_Inertia = true;
    float m_DecelerationRate = 0.135f;
    float m_ScrollSensitivity = 24.0f;
    Vec2 m_NormalizedPosition = { 0.0f, 1.0f };
    Vec2 m_ScrollOffset = { 0.0f, 0.0f };
    Vec2 m_ContentSize = { 300.0f, 600.0f };
    bool m_ShowHorizontalScrollbar = false;
    bool m_ShowVerticalScrollbar = true;
    Color m_BackgroundColor = { 28, 30, 38, 210 };
    Color m_ScrollbarTrackColor = { 22, 24, 30, 220 };
    Color m_ScrollbarThumbColor = { 95, 105, 125, 240 };
    bool m_RaycastTarget = true;

    CScrollViewComponent()
        : IComponent(COMPONENT_SCROLL_VIEW, "Scroll View")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_SCROLL_VIEW;
    }

    std::string GetTypeName() const override
    {
        return "Scroll View";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CHorizontalLayoutComponent : public IComponent
{
public:
    SUiLayoutPadding m_Padding;
    float m_Spacing = 8.0f;
    EUiLayoutChildAlignment m_ChildAlignment = EUiLayoutChildAlignment::UPPER_LEFT;
    bool m_ReverseArrangement = false;
    bool m_ChildControlWidth = true;
    bool m_ChildControlHeight = true;
    bool m_ChildScaleWidth = false;
    bool m_ChildScaleHeight = false;
    bool m_ChildForceExpandWidth = true;
    bool m_ChildForceExpandHeight = true;

    CHorizontalLayoutComponent()
        : IComponent(COMPONENT_HORIZONTAL_LAYOUT, "Horizontal Layout Group")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_HORIZONTAL_LAYOUT;
    }

    std::string GetTypeName() const override
    {
        return "Horizontal Layout Group";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CVerticalLayoutComponent : public IComponent
{
public:
    SUiLayoutPadding m_Padding;
    float m_Spacing = 8.0f;
    EUiLayoutChildAlignment m_ChildAlignment = EUiLayoutChildAlignment::UPPER_LEFT;
    bool m_ReverseArrangement = false;
    bool m_ChildControlWidth = true;
    bool m_ChildControlHeight = true;
    bool m_ChildScaleWidth = false;
    bool m_ChildScaleHeight = false;
    bool m_ChildForceExpandWidth = true;
    bool m_ChildForceExpandHeight = true;

    CVerticalLayoutComponent()
        : IComponent(COMPONENT_VERTICAL_LAYOUT, "Vertical Layout Group")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_VERTICAL_LAYOUT;
    }

    std::string GetTypeName() const override
    {
        return "Vertical Layout Group";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CGridLayoutComponent : public IComponent
{
public:
    SUiLayoutPadding m_Padding;
    Vec2 m_CellSize = { 100.0f, 100.0f };
    Vec2 m_Spacing = { 8.0f, 8.0f };
    EUiGridStartCorner m_StartCorner = EUiGridStartCorner::UPPER_LEFT;
    EUiGridStartAxis m_StartAxis = EUiGridStartAxis::HORIZONTAL;
    EUiLayoutChildAlignment m_ChildAlignment = EUiLayoutChildAlignment::UPPER_LEFT;
    EUiGridConstraint m_Constraint = EUiGridConstraint::FLEXIBLE;
    int m_ConstraintCount = 2;

    CGridLayoutComponent()
        : IComponent(COMPONENT_GRID_LAYOUT, "Grid Layout Group")
    {
    }

    EComponentType GetType() const override
    {
        return COMPONENT_GRID_LAYOUT;
    }

    std::string GetTypeName() const override
    {
        return "Grid Layout Group";
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CUiComponent : public IComponent
{
public:
    EGuiWidgetType m_WidgetType = EGuiWidgetType::BUTTON;
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
            case EGuiWidgetType::BUTTON:
                return "Ui Button";
            case EGuiWidgetType::INPUT_FIELD:
                return "Ui Inputfield";
            case EGuiWidgetType::COMBO:
                return "Ui Combo";
        }
        return "Ui Button";
    }

    void SyncToRuntimeDesc(const CUiRectTransformComponent* pRectTransform,
                           const CTextComponent* pTextComponent)
    {
        m_RuntimeDesc.m_WidgetId = reinterpret_cast<uint64_t>(this);
        m_RuntimeDesc.m_WidgetType = m_WidgetType;
        if (pRectTransform)
        {
            m_RuntimeDesc.m_Position = pRectTransform->m_Position;
            m_RuntimeDesc.m_Size = pRectTransform->m_Size;
        }
        else
        {
            m_RuntimeDesc.m_Position = { 40.0f, 40.0f };
            m_RuntimeDesc.m_Size = { 160.0f, 30.0f };
        }
        const bool textEnabled = (pTextComponent != nullptr && pTextComponent->m_Enabled);
        m_RuntimeDesc.m_TextEnabled = textEnabled;
        if (textEnabled)
        {
            m_RuntimeDesc.m_Text = pTextComponent->m_Text;
            m_RuntimeDesc.m_TextFont = pTextComponent->m_Font;
            m_RuntimeDesc.m_TextFontStyle = static_cast<int>(pTextComponent->m_FontStyle);
            m_RuntimeDesc.m_TextFontSize = pTextComponent->m_FontSize;
            m_RuntimeDesc.m_TextLineSpacing = pTextComponent->m_LineSpacing;
            m_RuntimeDesc.m_TextRichText = pTextComponent->m_RichText;
            m_RuntimeDesc.m_TextColor = pTextComponent->m_Color;
            m_RuntimeDesc.m_TextHorizontalAlignment = static_cast<int>(pTextComponent->m_HorizontalAlignment);
            m_RuntimeDesc.m_TextVerticalAlignment = static_cast<int>(pTextComponent->m_VerticalAlignment);
            m_RuntimeDesc.m_TextHorizontalOverflow = static_cast<int>(pTextComponent->m_HorizontalOverflow);
            m_RuntimeDesc.m_TextVerticalOverflow = static_cast<int>(pTextComponent->m_VerticalOverflow);
            m_RuntimeDesc.m_TextBestFit = pTextComponent->m_BestFit;
            m_RuntimeDesc.m_TextMaterial = pTextComponent->m_Material;
        }
        else
        {
            m_RuntimeDesc.m_Text.clear();
            m_RuntimeDesc.m_TextFont.clear();
            m_RuntimeDesc.m_TextFontStyle = 0;
            m_RuntimeDesc.m_TextFontSize = 16.0f;
            m_RuntimeDesc.m_TextLineSpacing = 1.0f;
            m_RuntimeDesc.m_TextRichText = false;
            m_RuntimeDesc.m_TextColor = { 235, 235, 235, 255 };
            m_RuntimeDesc.m_TextHorizontalAlignment = 1;
            m_RuntimeDesc.m_TextVerticalAlignment = 1;
            m_RuntimeDesc.m_TextHorizontalOverflow = 0;
            m_RuntimeDesc.m_TextVerticalOverflow = 1;
            m_RuntimeDesc.m_TextBestFit = false;
            m_RuntimeDesc.m_TextMaterial.clear();
        }
        m_RuntimeDesc.m_Placeholder = m_Placeholder;
        m_RuntimeDesc.m_vItems = m_vItems;
        m_RuntimeDesc.m_SelectedIndex = m_SelectedIndex;
        m_RuntimeDesc.m_ButtonInteractable = m_Interactable;
        m_RuntimeDesc.m_ButtonTransition = m_Transition;
        m_RuntimeDesc.m_ButtonNormalColor = m_NormalColor;
        m_RuntimeDesc.m_ButtonHighlightedColor = m_HighlightedColor;
        m_RuntimeDesc.m_ButtonPressedColor = m_PressedColor;
        m_RuntimeDesc.m_ButtonSelectedColor = m_SelectedColor;
        m_RuntimeDesc.m_ButtonDisabledColor = m_DisabledColor;
        m_RuntimeDesc.m_ButtonColorMultiplier = m_ColorMultiplier;
        m_RuntimeDesc.m_ButtonFadeDuration = m_FadeDuration;
        m_RuntimeDesc.m_ButtonNavigation = m_Navigation;
        m_RuntimeDesc.m_ShowNavigationVisualization = m_ShowNavigationVisualization;
    }

    void SyncFromRuntimeDesc(CTextComponent* pTextComponent)
    {
        if (pTextComponent && pTextComponent->m_Enabled)
        {
            pTextComponent->m_Text = m_RuntimeDesc.m_Text;
        }
        m_SelectedIndex = m_RuntimeDesc.m_SelectedIndex;
    }

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;
};

class CComponentManager
{
private:
    std::vector<std::shared_ptr<IComponent>> m_vComponents;
    std::array<IComponent*, COMPONENT_COUNT> m_apFastLookup = {};

    template <typename T>
    static constexpr EComponentType StaticComponentType()
    {
        if constexpr (std::is_same_v<T, CTransformComponent>)
        {
            return COMPONENT_TRANSFORM;
        }
        else if constexpr (std::is_same_v<T, CMeshComponent>)
        {
            return COMPONENT_MESH;
        }
        else if constexpr (std::is_same_v<T, CMaterialComponent>)
        {
            return COMPONENT_MATERIAL;
        }
        else if constexpr (std::is_same_v<T, CLightComponent>)
        {
            return COMPONENT_LIGHT;
        }
        else if constexpr (std::is_same_v<T, CCollisionComponent>)
        {
            return COMPONENT_COLLISION;
        }
        else if constexpr (std::is_same_v<T, CCameraComponent>)
        {
            return COMPONENT_CAMERA;
        }
        else if constexpr (std::is_same_v<T, CScriptComponent>)
        {
            return COMPONENT_SCRIPT;
        }
        else if constexpr (std::is_same_v<T, CUiComponent>)
        {
            return COMPONENT_UI;
        }
        else if constexpr (std::is_same_v<T, CTextComponent>)
        {
            return COMPONENT_TEXT;
        }
        else if constexpr (std::is_same_v<T, CUiRectTransformComponent>)
        {
            return COMPONENT_UI_RECT_TRANSFORM;
        }
        else if constexpr (std::is_same_v<T, CCanvasComponent>)
        {
            return COMPONENT_CANVAS;
        }
        else if constexpr (std::is_same_v<T, CImageComponent>)
        {
            return COMPONENT_IMAGE;
        }
        else if constexpr (std::is_same_v<T, CPanelComponent>)
        {
            return COMPONENT_PANEL;
        }
        else if constexpr (std::is_same_v<T, CButtonComponent>)
        {
            return COMPONENT_BUTTON;
        }
        else if constexpr (std::is_same_v<T, CSliderComponent>)
        {
            return COMPONENT_SLIDER;
        }
        else if constexpr (std::is_same_v<T, CToggleComponent>)
        {
            return COMPONENT_TOGGLE;
        }
        else if constexpr (std::is_same_v<T, CProgressBarComponent>)
        {
            return COMPONENT_PROGRESS_BAR;
        }
        else if constexpr (std::is_same_v<T, CScrollViewComponent>)
        {
            return COMPONENT_SCROLL_VIEW;
        }
        else if constexpr (std::is_same_v<T, CHorizontalLayoutComponent>)
        {
            return COMPONENT_HORIZONTAL_LAYOUT;
        }
        else if constexpr (std::is_same_v<T, CVerticalLayoutComponent>)
        {
            return COMPONENT_VERTICAL_LAYOUT;
        }
        else if constexpr (std::is_same_v<T, CGridLayoutComponent>)
        {
            return COMPONENT_GRID_LAYOUT;
        }
        else
        {
            return COMPONENT_CUSTOM;
        }
    }

public:
    void RebuildFastLookup()
    {
        m_apFastLookup.fill(nullptr);
        for (const auto& pComp : m_vComponents)
        {
            if (!pComp)
            {
                continue;
            }
            const int typeIdx = static_cast<int>(pComp->GetType());
            if (typeIdx >= 0 && typeIdx < static_cast<int>(COMPONENT_COUNT) &&
                m_apFastLookup[static_cast<size_t>(typeIdx)] == nullptr)
            {
                m_apFastLookup[static_cast<size_t>(typeIdx)] = pComp.get();
            }
        }
    }

    template <typename T>
    T* GetFastComponent(EComponentType type) const
    {
        const int typeIdx = static_cast<int>(type);
        if (typeIdx >= 0 && typeIdx < static_cast<int>(COMPONENT_COUNT))
        {
            return static_cast<T*>(m_apFastLookup[static_cast<size_t>(typeIdx)]);
        }
        return nullptr;
    }

    void AddComponent(std::shared_ptr<IComponent> pComponent)
    {
        if (pComponent)
        {
            const int typeIdx = static_cast<int>(pComponent->GetType());
            if (typeIdx >= 0 && typeIdx < static_cast<int>(COMPONENT_COUNT) &&
                m_apFastLookup[static_cast<size_t>(typeIdx)] == nullptr)
            {
                m_apFastLookup[static_cast<size_t>(typeIdx)] = pComponent.get();
            }
        }
        m_vComponents.push_back(std::move(pComponent));
    }

    void RemoveComponent(size_t index)
    {
        if (index < m_vComponents.size())
        {
            m_vComponents.erase(m_vComponents.begin() + index);
            RebuildFastLookup();
        }
    }

    void RemoveMeshAndMaterialForUi(bool keepTransform = false)
    {
        for (size_t i = m_vComponents.size(); i > 0; --i)
        {
            const auto& pComponent = m_vComponents[i - 1];
            if (pComponent)
            {
                const EComponentType compType = pComponent->GetType();
                if (compType == COMPONENT_MESH || compType == COMPONENT_MATERIAL)
                {
                    if (compType == COMPONENT_MESH)
                    {
                        static_cast<CMeshComponent*>(pComponent.get())->ReleaseOwnedResources();
                    }
                    m_vComponents.erase(m_vComponents.begin() + static_cast<std::ptrdiff_t>(i - 1));
                }
                else if (!keepTransform && compType == COMPONENT_TRANSFORM)
                {
                    m_vComponents.erase(m_vComponents.begin() + static_cast<std::ptrdiff_t>(i - 1));
                }
            }
        }
        for (size_t i = 0; i < m_vComponents.size(); ++i)
        {
            if (m_vComponents[i] && m_vComponents[i]->GetType() == COMPONENT_UI_RECT_TRANSFORM)
            {
                if (i > 0)
                {
                    auto pRectTransform = m_vComponents[i];
                    m_vComponents.erase(m_vComponents.begin() + static_cast<std::ptrdiff_t>(i));
                    m_vComponents.insert(m_vComponents.begin(), pRectTransform);
                }
                break;
            }
        }
        RebuildFastLookup();
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

    template <typename T>
    std::shared_ptr<T> GetComponentOfType()
    {
        constexpr EComponentType targetType = StaticComponentType<T>();
        if constexpr (targetType != COMPONENT_CUSTOM)
        {
            for (auto& pComp : m_vComponents)
            {
                if (pComp && pComp->GetType() == targetType)
                {
                    return std::static_pointer_cast<T>(pComp);
                }
            }
            return nullptr;
        }
        else
        {
            for (auto& pComp : m_vComponents)
            {
                auto casted = std::dynamic_pointer_cast<T>(pComp);
                if (casted)
                {
                    return casted;
                }
            }
            return nullptr;
        }
    }

    CTransformComponent* GetTransform() const
    {
        return GetFastComponent<CTransformComponent>(COMPONENT_TRANSFORM);
    }

    CMeshComponent* GetMesh() const
    {
        return GetFastComponent<CMeshComponent>(COMPONENT_MESH);
    }

    CLightComponent* GetLight() const
    {
        return GetFastComponent<CLightComponent>(COMPONENT_LIGHT);
    }

    CMaterialComponent* GetMaterial() const
    {
        return GetFastComponent<CMaterialComponent>(COMPONENT_MATERIAL);
    }

    CCollisionComponent* GetCollision() const
    {
        return GetFastComponent<CCollisionComponent>(COMPONENT_COLLISION);
    }

    CRigidbodyComponent* GetRigidbody() const
    {
        return GetFastComponent<CRigidbodyComponent>(COMPONENT_RIGIDBODY);
    }

    CCameraComponent* GetCamera() const
    {
        return GetFastComponent<CCameraComponent>(COMPONENT_CAMERA);
    }

    CUiComponent* GetUiComponent() const
    {
        return GetFastComponent<CUiComponent>(COMPONENT_UI);
    }

    CCanvasComponent* GetCanvas() const
    {
        return GetFastComponent<CCanvasComponent>(COMPONENT_CANVAS);
    }

    CUiRectTransformComponent* GetRectTransform() const
    {
        return GetFastComponent<CUiRectTransformComponent>(COMPONENT_UI_RECT_TRANSFORM);
    }

    CImageComponent* GetImage() const
    {
        return GetFastComponent<CImageComponent>(COMPONENT_IMAGE);
    }

    CPanelComponent* GetPanel() const
    {
        return GetFastComponent<CPanelComponent>(COMPONENT_PANEL);
    }

    CButtonComponent* GetButton() const
    {
        return GetFastComponent<CButtonComponent>(COMPONENT_BUTTON);
    }

    CTextComponent* GetText() const
    {
        return GetFastComponent<CTextComponent>(COMPONENT_TEXT);
    }

    CSliderComponent* GetSlider() const
    {
        return GetFastComponent<CSliderComponent>(COMPONENT_SLIDER);
    }

    CToggleComponent* GetToggle() const
    {
        return GetFastComponent<CToggleComponent>(COMPONENT_TOGGLE);
    }

    CProgressBarComponent* GetProgressBar() const
    {
        return GetFastComponent<CProgressBarComponent>(COMPONENT_PROGRESS_BAR);
    }

    CScrollViewComponent* GetScrollView() const
    {
        return GetFastComponent<CScrollViewComponent>(COMPONENT_SCROLL_VIEW);
    }

    CHorizontalLayoutComponent* GetHorizontalLayout() const
    {
        return GetFastComponent<CHorizontalLayoutComponent>(COMPONENT_HORIZONTAL_LAYOUT);
    }

    CVerticalLayoutComponent* GetVerticalLayout() const
    {
        return GetFastComponent<CVerticalLayoutComponent>(COMPONENT_VERTICAL_LAYOUT);
    }

    CGridLayoutComponent* GetGridLayout() const
    {
        return GetFastComponent<CGridLayoutComponent>(COMPONENT_GRID_LAYOUT);
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
        for (const auto& pComp : m_vComponents)
        {
            nlohmann::json compJson;
            compJson["type"] = pComp->GetTypeName();
            compJson["enabled"] = pComp->m_Enabled;
            nlohmann::json data;
            pComp->Serialize(data);
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
        m_Name = "3D Text";
        m_Type = COMPONENT_CUSTOM;
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
        if (j.contains("text"))
        {
            m_Text = j["text"];
        }
        if (j.contains("size"))
        {
            m_Size = j["size"];
        }
        if (j.contains("thickness"))
        {
            m_Thickness = j["thickness"];
        }
        if (j.contains("letter_spacing"))
        {
            m_LetterSpacing = j["letter_spacing"];
        }
        if (j.contains("font_path"))
        {
            m_FontPath = j["font_path"];
        }
    }
};

#endif // __COMPONENT_H__
