#include "component.h"
#include "scripting/script_component.h"
#define _CRT_SECURE_NO_WARNINGS

#include "engine/component_factory_registry.h"
#include "entity.h"
#include "nlohmann/json.hpp"

#include <algorithm>

void CMeshComponent::Serialize(nlohmann::json& json) const
{
    json["segments"] = m_Segments;
    json["type"] = static_cast<int>(m_Type);

    if (!m_AssetName.empty())
    {
        json["asset_name"] = m_AssetName;
    }

    json["is_editable_mesh"] = m_IsEditableMesh;
    json["editable_vertices"] = nlohmann::json::array();

    for (auto& v : m_EditableMesh.m_vVertices)
    {
        json["editable_vertices"].push_back({ v.m_Position.x,
                                              v.m_Position.y,
                                              v.m_Position.z,
                                              v.m_U,
                                              v.m_V });
    }

    json["editable_triangles"] = nlohmann::json::array();

    for (auto& t : m_EditableMesh.m_vTriangles)
    {
        json["editable_triangles"].push_back({ t.m_A,
                                               t.m_B,
                                               t.m_C });
    }
}

void CMeshComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("segments"))
    {
        m_Segments = json["segments"];
    }
    if (json.contains("type"))
    {
        m_Type = static_cast<EObjectType>(json["type"].get<int>());
    }

    if (json.contains("asset_name"))
    {
        m_AssetName = json["asset_name"];
    }

    if (json.contains("is_editable_mesh"))
    {
        m_IsEditableMesh = json["is_editable_mesh"];
    }

    if (json.contains("editable_vertices"))
    {
        m_EditableMesh.m_vVertices.clear();
        for (auto& v : json["editable_vertices"])
        {
            SEditableVertex vert;
            vert.m_Position = Vec3(v[0],
                                   v[1],
                                   v[2]);
            if (v.size() >= 5)
            {
                vert.m_U = v[3];
                vert.m_V = v[4];
            }

            m_EditableMesh.m_vVertices.push_back(vert);
        }
    }

    if (json.contains("editable_triangles"))
    {
        m_EditableMesh.m_vTriangles.clear();
        for (auto& t : json["editable_triangles"])
        {
            SEditableTriangle tri;

            tri.m_A = t[0];
            tri.m_B = t[1];
            tri.m_C = t[2];

            m_EditableMesh.m_vTriangles.push_back(tri);
        }
    }
}

void CLightComponent::Serialize(nlohmann::json& json) const
{
    json["light_enabled"] = m_Light.m_Enabled;
    json["light_position"] = { m_Light.m_Position.x, m_Light.m_Position.y, m_Light.m_Position.z };
    json["light_target"] = { m_Light.m_Target.x, m_Light.m_Target.y, m_Light.m_Target.z };
    json["light_rotation"] = { m_Light.m_Rotation.x, m_Light.m_Rotation.y, m_Light.m_Rotation.z };

    char aColorBuf[16];
    snprintf(aColorBuf, sizeof(aColorBuf), "%02X%02X%02X%02X", m_Light.m_Color.r, m_Light.m_Color.g, m_Light.m_Color.b, m_Light.m_Color.a);
    json["light_color"] = std::string(aColorBuf);

    json["light_intensity"] = m_Light.m_Intensity;
    json["light_range"] = m_Light.m_Range;
    json["light_spot_angle"] = m_Light.m_SpotAngle;
    json["light_type"] = m_Light.m_Light.type;
}

CLightComponent::CLightComponent() : IComponent(COMPONENT_LIGHT, "Light"), m_Created(false)
{
    m_Light = CreateLighting({ 0, 0, 0 }, WHITE);
}

void CLightComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("light_enabled"))
    {
        m_Light.m_Enabled = json["light_enabled"];
    }
    if (json.contains("light_position"))
    {
        auto& p = json["light_position"];
        m_Light.m_Position = Vec3(p[0], p[1], p[2]);
    }
    if (json.contains("light_target"))
    {
        auto& t = json["light_target"];
        m_Light.m_Target = Vec3(t[0], t[1], t[2]);
    }
    if (json.contains("light_rotation"))
    {
        auto& r = json["light_rotation"];
        m_Light.m_Rotation = Vec3(r[0], r[1], r[2]);
    }
    if (json.contains("light_color"))
    {
        std::string hex = json["light_color"];
        unsigned int rgba = std::stoul(hex, nullptr, 16);
        m_Light.m_Color = {
            static_cast<unsigned char>((rgba >> 24) & 0xFF),
            static_cast<unsigned char>((rgba >> 16) & 0xFF),
            static_cast<unsigned char>((rgba >> 8) & 0xFF),
            static_cast<unsigned char>(rgba & 0xFF)
        };
    }
    if (json.contains("light_intensity"))
    {
        m_Light.m_Intensity = json["light_intensity"];
    }
    if (json.contains("light_range"))
    {
        m_Light.m_Range = json["light_range"];
    }
    if (json.contains("light_spot_angle"))
    {
        m_Light.m_SpotAngle = json["light_spot_angle"];
    }
    if (json.contains("light_type"))
    {
        m_Light.m_Light.type = json["light_type"];
    }
}

void CLightComponent::OnEntityTransformChanged()
{
}

void CCollisionComponent::Serialize(nlohmann::json& json) const
{
    json["collider_type"] = static_cast<int>(m_ColliderType);
    json["is_trigger"] = m_IsTrigger;

    json["size"] = { m_Size.x, m_Size.y, m_Size.z };
    json["radius"] = m_Radius;
    json["height"] = m_Height;

    json["center"] = { m_Center.x, m_Center.y, m_Center.z };
}

void CCollisionComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("collider_type"))
    {
        m_ColliderType = static_cast<EColliderType>(json["collider_type"].get<int>());
    }
    if (json.contains("is_trigger"))
    {
        m_IsTrigger = json["is_trigger"];
    }

    if (json.contains("size"))
    {
        auto& s = json["size"];
        m_Size = Vec3(s[0], s[1], s[2]);
    }

    if (json.contains("radius"))
    {
        m_Radius = json["radius"];
    }
    if (json.contains("height"))
    {
        m_Height = json["height"];
    }

    if (json.contains("center"))
    {
        auto& c = json["center"];
        m_Center = Vec3(c[0], c[1], c[2]);
    }

    m_Dirty = true;
}

void CCollisionComponent::OnEntityTransformChanged()
{
    m_Dirty = true;
}

void CRigidbodyComponent::Serialize(nlohmann::json& json) const
{
    json["mass"] = m_Mass;
    json["linear_drag"] = m_LinearDrag;
    json["friction"] = m_Friction;
    json["restitution"] = m_Restitution;
    json["use_gravity"] = m_UseGravity;
    json["is_kinematic"] = m_IsKinematic;
    json["freeze_position_x"] = m_FreezePositionX;
    json["freeze_position_y"] = m_FreezePositionY;
    json["freeze_position_z"] = m_FreezePositionZ;
}

void CRigidbodyComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("mass"))
    {
        m_Mass = std::max(0.001f, json["mass"].get<float>());
    }
    if (json.contains("linear_drag"))
    {
        m_LinearDrag = std::max(0.0f, json["linear_drag"].get<float>());
    }
    if (json.contains("friction"))
    {
        m_Friction = std::clamp(json["friction"].get<float>(), 0.0f, 1.0f);
    }
    if (json.contains("restitution"))
    {
        m_Restitution = std::clamp(json["restitution"].get<float>(), 0.0f, 1.0f);
    }
    if (json.contains("use_gravity"))
    {
        m_UseGravity = json["use_gravity"].get<bool>();
    }
    if (json.contains("is_kinematic"))
    {
        m_IsKinematic = json["is_kinematic"].get<bool>();
    }
    if (json.contains("freeze_position_x"))
    {
        m_FreezePositionX = json["freeze_position_x"].get<bool>();
    }
    if (json.contains("freeze_position_y"))
    {
        m_FreezePositionY = json["freeze_position_y"].get<bool>();
    }
    if (json.contains("freeze_position_z"))
    {
        m_FreezePositionZ = json["freeze_position_z"].get<bool>();
    }
    ResetRuntimeState();
}

namespace
{

    nlohmann::json SerializeGuiColor(Color c)
    {
        return nlohmann::json::array({ c.r, c.g, c.b, c.a });
    }

    void DeserializeGuiColor(const nlohmann::json& j, const char* key, Color& outColor)
    {
        if (j.contains(key) && j[key].is_array() && j[key].size() >= 4)
        {
            outColor.r = static_cast<unsigned char>(j[key][0].get<int>());
            outColor.g = static_cast<unsigned char>(j[key][1].get<int>());
            outColor.b = static_cast<unsigned char>(j[key][2].get<int>());
            outColor.a = static_cast<unsigned char>(j[key][3].get<int>());
        }
    }

} // anonymous

void CUiComponent::Serialize(nlohmann::json& json) const
{
    json["widget_type"] = static_cast<int>(m_WidgetType);
    json["placeholder"] = m_Placeholder;
    json["items"] = m_vItems;
    json["selected_index"] = m_SelectedIndex;
    json["interactable"] = m_Interactable;
    json["transition"] = static_cast<int>(m_Transition);
    json["normal_color"] = SerializeGuiColor(m_NormalColor);
    json["highlighted_color"] = SerializeGuiColor(m_HighlightedColor);
    json["pressed_color"] = SerializeGuiColor(m_PressedColor);
    json["selected_color"] = SerializeGuiColor(m_SelectedColor);
    json["disabled_color"] = SerializeGuiColor(m_DisabledColor);
    json["color_multiplier"] = m_ColorMultiplier;
    json["fade_duration"] = m_FadeDuration;
    json["navigation"] = static_cast<int>(m_Navigation);
    json["show_navigation_visualization"] = m_ShowNavigationVisualization;
}

void CUiComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("widget_type") && json["widget_type"].is_number_integer())
    {
        int wt = json["widget_type"].get<int>();
        if (wt >= 0 && wt <= 2)
        {
            m_WidgetType = static_cast<EGuiWidgetType>(wt);
        }
    }
    if (json.contains("placeholder") && json["placeholder"].is_string())
    {
        m_Placeholder = json["placeholder"].get<std::string>();
    }
    if (json.contains("items") && json["items"].is_array())
    {
        m_vItems.clear();
        for (const auto& item : json["items"])
        {
            if (item.is_string())
            {
                m_vItems.push_back(item.get<std::string>());
            }
        }
    }
    if (json.contains("selected_index") && json["selected_index"].is_number_integer())
    {
        m_SelectedIndex = json["selected_index"].get<int>();
    }
    if (json.contains("interactable") && json["interactable"].is_boolean())
    {
        m_Interactable = json["interactable"].get<bool>();
    }
    if (json.contains("transition") && json["transition"].is_number_integer())
    {
        const int transition = json["transition"].get<int>();
        if (transition >= static_cast<int>(EGuiButtonTransition::NONE) &&
            transition <= static_cast<int>(EGuiButtonTransition::COLOR_TINT))
        {
            m_Transition = static_cast<EGuiButtonTransition>(transition);
        }
    }
    DeserializeGuiColor(json, "normal_color", m_NormalColor);
    DeserializeGuiColor(json, "highlighted_color", m_HighlightedColor);
    DeserializeGuiColor(json, "pressed_color", m_PressedColor);
    DeserializeGuiColor(json, "selected_color", m_SelectedColor);
    DeserializeGuiColor(json, "disabled_color", m_DisabledColor);
    if (json.contains("color_multiplier") && json["color_multiplier"].is_number())
    {
        m_ColorMultiplier = std::clamp(json["color_multiplier"].get<float>(), 0.0f, 10.0f);
    }
    if (json.contains("fade_duration") && json["fade_duration"].is_number())
    {
        m_FadeDuration = std::clamp(json["fade_duration"].get<float>(), 0.0f, 10.0f);
    }
    if (json.contains("navigation") && json["navigation"].is_number_integer())
    {
        const int navigation = json["navigation"].get<int>();
        if (navigation >= static_cast<int>(EGuiButtonNavigation::AUTOMATIC) &&
            navigation <= static_cast<int>(EGuiButtonNavigation::NONE))
        {
            m_Navigation = static_cast<EGuiButtonNavigation>(navigation);
        }
    }
    if (json.contains("show_navigation_visualization") && json["show_navigation_visualization"].is_boolean())
    {
        m_ShowNavigationVisualization = json["show_navigation_visualization"].get<bool>();
    }
    m_Name = GetTypeName();
}

void CTextComponent::Serialize(nlohmann::json& json) const
{
    json["text"] = m_Text;
    json["font"] = m_Font;
    json["font_style"] = static_cast<int>(m_FontStyle);
    json["font_size"] = m_FontSize;
    json["line_spacing"] = m_LineSpacing;
    json["character_spacing"] = m_CharacterSpacing;
    json["rich_text"] = m_RichText;
    json["horizontal_alignment"] = static_cast<int>(m_HorizontalAlignment);
    json["vertical_alignment"] = static_cast<int>(m_VerticalAlignment);
    json["horizontal_overflow"] = static_cast<int>(m_HorizontalOverflow);
    json["vertical_overflow"] = static_cast<int>(m_VerticalOverflow);
    json["best_fit"] = m_BestFit;
    json["color"] = SerializeGuiColor(m_Color);
    json["material"] = m_Material;
    json["raycast_target"] = m_RaycastTarget;
}

void CTextComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("text") && json["text"].is_string())
    {
        m_Text = json["text"].get<std::string>();
    }
    if (json.contains("font") && json["font"].is_string())
    {
        m_Font = json["font"].get<std::string>();
    }
    if (json.contains("font_style") && json["font_style"].is_number_integer())
    {
        const int fontStyle = json["font_style"].get<int>();
        if (fontStyle >= static_cast<int>(EUiTextFontStyle::NORMAL) &&
            fontStyle <= static_cast<int>(EUiTextFontStyle::BOLD_ITALIC))
        {
            m_FontStyle = static_cast<EUiTextFontStyle>(fontStyle);
        }
    }
    if (json.contains("font_size") && json["font_size"].is_number())
    {
        m_FontSize = std::clamp(json["font_size"].get<float>(), 1.0f, 256.0f);
    }
    if (json.contains("line_spacing") && json["line_spacing"].is_number())
    {
        m_LineSpacing = std::clamp(json["line_spacing"].get<float>(), 0.1f, 10.0f);
    }
    if (json.contains("character_spacing") && json["character_spacing"].is_number())
    {
        m_CharacterSpacing = json["character_spacing"].get<float>();
    }
    if (json.contains("rich_text") && json["rich_text"].is_boolean())
    {
        m_RichText = json["rich_text"].get<bool>();
    }
    if (json.contains("horizontal_alignment") && json["horizontal_alignment"].is_number_integer())
    {
        const int alignment = json["horizontal_alignment"].get<int>();
        if (alignment >= static_cast<int>(EUiTextAlignment::LEFT) &&
            alignment <= static_cast<int>(EUiTextAlignment::RIGHT))
        {
            m_HorizontalAlignment = static_cast<EUiTextAlignment>(alignment);
        }
    }
    if (json.contains("vertical_alignment") && json["vertical_alignment"].is_number_integer())
    {
        const int alignment = json["vertical_alignment"].get<int>();
        if (alignment >= static_cast<int>(EUiTextVerticalAlignment::TOP) &&
            alignment <= static_cast<int>(EUiTextVerticalAlignment::BOTTOM))
        {
            m_VerticalAlignment = static_cast<EUiTextVerticalAlignment>(alignment);
        }
    }
    if (json.contains("horizontal_overflow") && json["horizontal_overflow"].is_number_integer())
    {
        const int overflow = json["horizontal_overflow"].get<int>();
        if (overflow >= static_cast<int>(EUiTextOverflow::WRAP) &&
            overflow <= static_cast<int>(EUiTextOverflow::TRUNCATE))
        {
            m_HorizontalOverflow = static_cast<EUiTextOverflow>(overflow);
        }
    }
    if (json.contains("vertical_overflow") && json["vertical_overflow"].is_number_integer())
    {
        const int overflow = json["vertical_overflow"].get<int>();
        if (overflow >= static_cast<int>(EUiTextOverflow::WRAP) &&
            overflow <= static_cast<int>(EUiTextOverflow::TRUNCATE))
        {
            m_VerticalOverflow = static_cast<EUiTextOverflow>(overflow);
        }
    }
    if (json.contains("best_fit") && json["best_fit"].is_boolean())
    {
        m_BestFit = json["best_fit"].get<bool>();
    }
    DeserializeGuiColor(json, "color", m_Color);
    if (json.contains("material") && json["material"].is_string())
    {
        m_Material = json["material"].get<std::string>();
    }
    if (json.contains("raycast_target") && json["raycast_target"].is_boolean())
    {
        m_RaycastTarget = json["raycast_target"].get<bool>();
    }
}

CCanvasComponent::~CCanvasComponent()
{
    ReleaseOwnedResources();
}

void CCanvasComponent::ReleaseOwnedResources()
{
    if (m_RuntimeTexture.id != 0)
    {
        UnloadRenderTexture(m_RuntimeTexture);
        m_RuntimeTexture = {};
    }
    for (RenderTexture2D& screenSpaceTexture : m_aScreenSpaceTextures)
    {
        if (screenSpaceTexture.id != 0)
        {
            UnloadRenderTexture(screenSpaceTexture);
            screenSpaceTexture = {};
        }
    }
    if (m_QuadModelInitialized)
    {
        UnloadModel(m_RuntimeQuadModel);
        m_RuntimeQuadModel = {};
        m_QuadModelInitialized = false;
    }
}

void CCanvasComponent::Serialize(nlohmann::json& json) const
{
    json["render_mode"] = static_cast<int>(m_RenderMode);
    json["render_camera_id"] = m_RenderCameraEntityId;
    json["pixel_perfect"] = m_PixelPerfect;
    json["sorting_order"] = m_SortingOrder;
    json["target_display"] = m_TargetDisplay;
    json["reference_resolution"] = { m_ReferenceResolution.x, m_ReferenceResolution.y };
    json["match_width_or_height"] = m_MatchWidthOrHeight;
    json["width"] = m_Width;
    json["height"] = m_Height;
    json["scale_factor"] = m_ScaleFactor;
    json["plane_distance"] = m_PlaneDistance;
    json["event_camera_id"] = m_EventCameraEntityId;
    json["override_sorting"] = m_OverrideSorting;
}

void CCanvasComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("render_mode") && json["render_mode"].is_number_integer())
    {
        const int mode = json["render_mode"].get<int>();
        if (mode >= static_cast<int>(ECanvasRenderMode::SCREEN_SPACE_OVERLAY) &&
            mode <= static_cast<int>(ECanvasRenderMode::WORLD_SPACE))
        {
            m_RenderMode = static_cast<ECanvasRenderMode>(mode);
        }
    }
    if (json.contains("render_camera_id") && json["render_camera_id"].is_number_integer())
    {
        m_RenderCameraEntityId = json["render_camera_id"].get<int>();
    }
    if (json.contains("pixel_perfect") && json["pixel_perfect"].is_boolean())
    {
        m_PixelPerfect = json["pixel_perfect"].get<bool>();
    }
    if (json.contains("sorting_order") && json["sorting_order"].is_number_integer())
    {
        m_SortingOrder = json["sorting_order"].get<int>();
    }
    if (json.contains("target_display") && json["target_display"].is_number_integer())
    {
        m_TargetDisplay = json["target_display"].get<int>();
    }
    if (json.contains("reference_resolution") && json["reference_resolution"].is_array() &&
        json["reference_resolution"].size() >= 2)
    {
        m_ReferenceResolution = {
            json["reference_resolution"][0].get<float>(),
            json["reference_resolution"][1].get<float>()
        };
    }
    if (json.contains("match_width_or_height") && json["match_width_or_height"].is_number())
    {
        m_MatchWidthOrHeight = std::clamp(
            json["match_width_or_height"].get<float>(), 0.0f, 1.0f);
    }
    if (json.contains("width") && json["width"].is_number())
    {
        m_Width = json["width"].get<float>();
    }
    if (json.contains("height") && json["height"].is_number())
    {
        m_Height = json["height"].get<float>();
    }
    if (json.contains("scale_factor") && json["scale_factor"].is_number())
    {
        m_ScaleFactor = json["scale_factor"].get<float>();
    }
    if (json.contains("plane_distance") && json["plane_distance"].is_number())
    {
        m_PlaneDistance = json["plane_distance"].get<float>();
    }
    if (json.contains("event_camera_id") && json["event_camera_id"].is_number_integer())
    {
        m_EventCameraEntityId = json["event_camera_id"].get<int>();
    }
    if (json.contains("override_sorting") && json["override_sorting"].is_boolean())
    {
        m_OverrideSorting = json["override_sorting"].get<bool>();
    }
}

Rectangle CUiRectTransformComponent::CalculateLocalRect(const Vec2& parentSize) const
{
    Rectangle rect = { 0.0f, 0.0f, m_Size.x, m_Size.y };
    if (m_AnchorMin.x == m_AnchorMax.x && m_AnchorMin.y == m_AnchorMax.y)
    {
        const float anchorX = parentSize.x * m_AnchorMin.x;
        const float anchorY = parentSize.y * m_AnchorMin.y;
        const float pivotPointX = anchorX + m_AnchoredPosition.x;
        const float pivotPointY = anchorY + m_AnchoredPosition.y;
        rect.x = pivotPointX - m_Size.x * m_Pivot.x;
        rect.y = pivotPointY - m_Size.y * m_Pivot.y;
        rect.width = m_Size.x;
        rect.height = m_Size.y;
    }
    else
    {
        const float anchorMinX = parentSize.x * m_AnchorMin.x;
        const float anchorMinY = parentSize.y * m_AnchorMin.y;
        const float anchorMaxX = parentSize.x * m_AnchorMax.x;
        const float anchorMaxY = parentSize.y * m_AnchorMax.y;
        rect.x = anchorMinX + m_OffsetMin.x;
        rect.y = anchorMinY + m_OffsetMin.y;
        rect.width = std::max(1.0f, (anchorMaxX + m_OffsetMax.x) - rect.x);
        rect.height = std::max(1.0f, (anchorMaxY + m_OffsetMax.y) - rect.y);
    }
    return rect;
}

void CUiRectTransformComponent::SetAnchoredPosition(const Vec2& anchoredPosition)
{
    const Vec2 delta = anchoredPosition - m_AnchoredPosition;
    if (m_AnchorMin.x != m_AnchorMax.x)
    {
        m_OffsetMin.x += delta.x;
        m_OffsetMax.x += delta.x;
    }
    if (m_AnchorMin.y != m_AnchorMax.y)
    {
        m_OffsetMin.y += delta.y;
        m_OffsetMax.y += delta.y;
    }
    m_AnchoredPosition = anchoredPosition;
    m_Position = anchoredPosition;
}

void CUiRectTransformComponent::Serialize(nlohmann::json& json) const
{
    json["position"] = { m_AnchoredPosition.x, m_AnchoredPosition.y };
    json["anchored_position"] = { m_AnchoredPosition.x, m_AnchoredPosition.y };
    json["size"] = { m_Size.x, m_Size.y };
    json["anchor_min"] = { m_AnchorMin.x, m_AnchorMin.y };
    json["anchor_max"] = { m_AnchorMax.x, m_AnchorMax.y };
    json["pivot"] = { m_Pivot.x, m_Pivot.y };
    json["rotation_2d"] = m_Rotation2D;
    json["scale_2d"] = { m_Scale2D.x, m_Scale2D.y };
    json["offset_min"] = { m_OffsetMin.x, m_OffsetMin.y };
    json["offset_max"] = { m_OffsetMax.x, m_OffsetMax.y };
    json["clip_children"] = m_ClipChildren;
}

void CUiRectTransformComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("anchored_position") && json["anchored_position"].is_array() &&
        json["anchored_position"].size() >= 2)
    {
        m_AnchoredPosition = {
            json["anchored_position"][0].get<float>(),
            json["anchored_position"][1].get<float>()
        };
        m_Position = m_AnchoredPosition;
    }
    else if (json.contains("position") && json["position"].is_array() && json["position"].size() >= 2)
    {
        m_Position = { json["position"][0].get<float>(), json["position"][1].get<float>() };
        m_AnchoredPosition = m_Position;
    }

    if (json.contains("size") && json["size"].is_array() && json["size"].size() >= 2)
    {
        m_Size = { json["size"][0].get<float>(), json["size"][1].get<float>() };
    }
    if (json.contains("anchor_min") && json["anchor_min"].is_array() && json["anchor_min"].size() >= 2)
    {
        m_AnchorMin = { json["anchor_min"][0].get<float>(), json["anchor_min"][1].get<float>() };
    }
    if (json.contains("anchor_max") && json["anchor_max"].is_array() && json["anchor_max"].size() >= 2)
    {
        m_AnchorMax = { json["anchor_max"][0].get<float>(), json["anchor_max"][1].get<float>() };
    }
    if (json.contains("pivot") && json["pivot"].is_array() && json["pivot"].size() >= 2)
    {
        m_Pivot = { json["pivot"][0].get<float>(), json["pivot"][1].get<float>() };
    }
    if (json.contains("rotation_2d") && json["rotation_2d"].is_number())
    {
        m_Rotation2D = json["rotation_2d"].get<float>();
    }
    if (json.contains("scale_2d") && json["scale_2d"].is_array() && json["scale_2d"].size() >= 2)
    {
        m_Scale2D = { json["scale_2d"][0].get<float>(), json["scale_2d"][1].get<float>() };
    }
    if (json.contains("offset_min") && json["offset_min"].is_array() && json["offset_min"].size() >= 2)
    {
        m_OffsetMin = { json["offset_min"][0].get<float>(), json["offset_min"][1].get<float>() };
    }
    if (json.contains("offset_max") && json["offset_max"].is_array() && json["offset_max"].size() >= 2)
    {
        m_OffsetMax = { json["offset_max"][0].get<float>(), json["offset_max"][1].get<float>() };
    }
    if (json.contains("clip_children") && json["clip_children"].is_boolean())
    {
        m_ClipChildren = json["clip_children"].get<bool>();
    }
}

CImageComponent::~CImageComponent()
{
    ReleaseOwnedResources();
}

void CImageComponent::ReleaseOwnedResources()
{
    if (m_TextureLoaded && m_RuntimeTexture.id != 0)
    {
        UnloadTexture(m_RuntimeTexture);
        m_RuntimeTexture = {};
        m_TextureLoaded = false;
    }
    m_LoadedTexturePath.clear();
}

void CImageComponent::Serialize(nlohmann::json& json) const
{
    json["source_texture"] = m_SourceTexture;
    json["color"] = SerializeGuiColor(m_Color);
    json["material"] = m_Material;
    json["raycast_target"] = m_RaycastTarget;
    json["image_type"] = static_cast<int>(m_ImageType);
    json["preserve_aspect"] = m_PreserveAspect;
    json["fill_center"] = m_FillCenter;
    json["fill_amount"] = m_FillAmount;
    json["border"] = m_aBorder;
}

void CImageComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("source_texture") && json["source_texture"].is_string())
    {
        m_SourceTexture = json["source_texture"].get<std::string>();
    }
    DeserializeGuiColor(json, "color", m_Color);
    if (json.contains("material") && json["material"].is_string())
    {
        m_Material = json["material"].get<std::string>();
    }
    if (json.contains("raycast_target") && json["raycast_target"].is_boolean())
    {
        m_RaycastTarget = json["raycast_target"].get<bool>();
    }
    if (json.contains("image_type") && json["image_type"].is_number_integer())
    {
        const int type = json["image_type"].get<int>();
        if (type >= static_cast<int>(EUiImageType::SIMPLE) &&
            type <= static_cast<int>(EUiImageType::FILLED))
        {
            m_ImageType = static_cast<EUiImageType>(type);
        }
    }
    if (json.contains("preserve_aspect") && json["preserve_aspect"].is_boolean())
    {
        m_PreserveAspect = json["preserve_aspect"].get<bool>();
    }
    if (json.contains("fill_center") && json["fill_center"].is_boolean())
    {
        m_FillCenter = json["fill_center"].get<bool>();
    }
    if (json.contains("fill_amount") && json["fill_amount"].is_number())
    {
        m_FillAmount = json["fill_amount"].get<float>();
    }
    if (json.contains("border") && json["border"].is_array() && json["border"].size() >= 4)
    {
        for (std::size_t borderIndex = 0; borderIndex < m_aBorder.size(); ++borderIndex)
        {
            if (json["border"][borderIndex].is_number())
            {
                m_aBorder[borderIndex] =
                    std::max(0.0f, json["border"][borderIndex].get<float>());
            }
        }
    }
}

CPanelComponent::~CPanelComponent()
{
    ReleaseOwnedResources();
}

void CPanelComponent::ReleaseOwnedResources()
{
    if (m_TextureLoaded && m_RuntimeTexture.id != 0)
    {
        UnloadTexture(m_RuntimeTexture);
        m_RuntimeTexture = {};
        m_TextureLoaded = false;
    }
    m_LoadedTexturePath.clear();
}

void CPanelComponent::Serialize(nlohmann::json& json) const
{
    json["background_image"] = m_BackgroundImage;
    json["background_color"] = SerializeGuiColor(m_BackgroundColor);
    json["raycast_target"] = m_RaycastTarget;
}

void CPanelComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("background_image") && json["background_image"].is_string())
    {
        m_BackgroundImage = json["background_image"].get<std::string>();
    }
    DeserializeGuiColor(json, "background_color", m_BackgroundColor);
    if (json.contains("raycast_target") && json["raycast_target"].is_boolean())
    {
        m_RaycastTarget = json["raycast_target"].get<bool>();
    }
}

Color CButtonComponent::GetCurrentColor() const
{
    Color baseColor = m_NormalColor;
    if (!m_Interactable)
    {
        baseColor = m_DisabledColor;
    }
    else if (m_IsPressed)
    {
        baseColor = m_PressedColor;
    }
    else if (m_IsHovered)
    {
        baseColor = m_HighlightedColor;
    }
    else if (m_IsSelected)
    {
        baseColor = m_SelectedColor;
    }

    const float mult = std::clamp(m_ColorMultiplier, 0.0f, 5.0f);
    return {
        static_cast<unsigned char>(std::clamp(static_cast<int>(baseColor.r * mult), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(baseColor.g * mult), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(baseColor.b * mult), 0, 255)),
        baseColor.a
    };
}

void CButtonComponent::Serialize(nlohmann::json& json) const
{
    json["interactable"] = m_Interactable;
    json["transition"] = static_cast<int>(m_Transition);
    json["navigation"] = static_cast<int>(m_Navigation);
    json["normal_color"] = SerializeGuiColor(m_NormalColor);
    json["highlighted_color"] = SerializeGuiColor(m_HighlightedColor);
    json["pressed_color"] = SerializeGuiColor(m_PressedColor);
    json["selected_color"] = SerializeGuiColor(m_SelectedColor);
    json["disabled_color"] = SerializeGuiColor(m_DisabledColor);
    json["navigation_target_ids"] = m_aNavigationTargetIds;
    json["color_multiplier"] = m_ColorMultiplier;
    json["fade_duration"] = m_FadeDuration;
    json["sprite_swap_highlighted"] = m_SpriteSwapHighlighted;
    json["sprite_swap_pressed"] = m_SpriteSwapPressed;
    json["sprite_swap_selected"] = m_SpriteSwapSelected;
    json["sprite_swap_disabled"] = m_SpriteSwapDisabled;
    json["on_click_action"] = m_OnClickAction;
    json["event_callbacks"] = nlohmann::json::array();
    for (const SUiEventCallback& callback : m_vEventCallbacks)
    {
        json["event_callbacks"].push_back({
            { "event", static_cast<int>(callback.m_Event) },
            { "action", callback.m_Action }
        });
    }
    json["raycast_target"] = m_RaycastTarget;
}

void CButtonComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("interactable") && json["interactable"].is_boolean())
    {
        m_Interactable = json["interactable"].get<bool>();
    }
    if (json.contains("transition") && json["transition"].is_number_integer())
    {
        const int trans = json["transition"].get<int>();
        if (trans >= static_cast<int>(EUiTransition::NONE) &&
            trans <= static_cast<int>(EUiTransition::ANIMATION))
        {
            m_Transition = static_cast<EUiTransition>(trans);
        }
    }
    if (json.contains("navigation") && json["navigation"].is_number_integer())
    {
        const int nav = json["navigation"].get<int>();
        if (nav >= static_cast<int>(EUiNavigation::NONE) &&
            nav <= static_cast<int>(EUiNavigation::EXPLICIT))
        {
            m_Navigation = static_cast<EUiNavigation>(nav);
        }
    }
    DeserializeGuiColor(json, "normal_color", m_NormalColor);
    DeserializeGuiColor(json, "highlighted_color", m_HighlightedColor);
    DeserializeGuiColor(json, "pressed_color", m_PressedColor);
    DeserializeGuiColor(json, "selected_color", m_SelectedColor);
    DeserializeGuiColor(json, "disabled_color", m_DisabledColor);
    if (json.contains("navigation_target_ids") &&
        json["navigation_target_ids"].is_array() &&
        json["navigation_target_ids"].size() == m_aNavigationTargetIds.size())
    {
        for (size_t targetIndex = 0; targetIndex < m_aNavigationTargetIds.size(); ++targetIndex)
        {
            if (json["navigation_target_ids"][targetIndex].is_number_integer())
            {
                m_aNavigationTargetIds[targetIndex] =
                    json["navigation_target_ids"][targetIndex].get<int>();
            }
        }
    }
    if (json.contains("color_multiplier") && json["color_multiplier"].is_number())
    {
        m_ColorMultiplier = json["color_multiplier"].get<float>();
    }
    if (json.contains("fade_duration") && json["fade_duration"].is_number())
    {
        m_FadeDuration = json["fade_duration"].get<float>();
    }
    if (json.contains("sprite_swap_highlighted") && json["sprite_swap_highlighted"].is_string())
    {
        m_SpriteSwapHighlighted = json["sprite_swap_highlighted"].get<std::string>();
    }
    if (json.contains("sprite_swap_pressed") && json["sprite_swap_pressed"].is_string())
    {
        m_SpriteSwapPressed = json["sprite_swap_pressed"].get<std::string>();
    }
    if (json.contains("sprite_swap_selected") && json["sprite_swap_selected"].is_string())
    {
        m_SpriteSwapSelected = json["sprite_swap_selected"].get<std::string>();
    }
    if (json.contains("sprite_swap_disabled") && json["sprite_swap_disabled"].is_string())
    {
        m_SpriteSwapDisabled = json["sprite_swap_disabled"].get<std::string>();
    }
    if (json.contains("on_click_action") && json["on_click_action"].is_string())
    {
        m_OnClickAction = json["on_click_action"].get<std::string>();
    }
    if (json.contains("event_callbacks") && json["event_callbacks"].is_array())
    {
        m_vEventCallbacks.clear();
        for (const nlohmann::json& callbackJson : json["event_callbacks"])
        {
            if (!callbackJson.is_object() ||
                !callbackJson.contains("event") || !callbackJson["event"].is_number_integer() ||
                !callbackJson.contains("action") || !callbackJson["action"].is_string())
            {
                continue;
            }

            const int event = callbackJson["event"].get<int>();
            if (event < static_cast<int>(EUiEventType::POINTER_ENTER) ||
                event > static_cast<int>(EUiEventType::SUBMIT))
            {
                continue;
            }

            m_vEventCallbacks.push_back({
                static_cast<EUiEventType>(event),
                callbackJson["action"].get<std::string>()
            });
        }
    }
    if (json.contains("raycast_target") && json["raycast_target"].is_boolean())
    {
        m_RaycastTarget = json["raycast_target"].get<bool>();
    }
}

namespace
{
    nlohmann::json SerializeLayoutPadding(const SUiLayoutPadding& padding)
    {
        return {
            { "left", padding.m_Left },
            { "right", padding.m_Right },
            { "top", padding.m_Top },
            { "bottom", padding.m_Bottom }
        };
    }

    void DeserializeLayoutPadding(const nlohmann::json& json, const char* pKey, SUiLayoutPadding& outPadding)
    {
        if (!json.contains(pKey) || !json[pKey].is_object())
        {
            return;
        }
        const auto& padJson = json[pKey];
        outPadding.m_Left = padJson.value("left", outPadding.m_Left);
        outPadding.m_Right = padJson.value("right", outPadding.m_Right);
        outPadding.m_Top = padJson.value("top", outPadding.m_Top);
        outPadding.m_Bottom = padJson.value("bottom", outPadding.m_Bottom);
    }
} // anonymous

float CSliderComponent::GetNormalizedValue() const
{
    const float span = m_MaxValue - m_MinValue;
    if (std::abs(span) <= 1e-6f)
    {
        return 0.0f;
    }
    return std::clamp((m_Value - m_MinValue) / span, 0.0f, 1.0f);
}

void CSliderComponent::SetNormalizedValue(float normalized)
{
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    float val = m_MinValue + clamped * (m_MaxValue - m_MinValue);
    if (m_WholeNumbers)
    {
        val = std::round(val);
    }
    m_Value = std::clamp(val, std::min(m_MinValue, m_MaxValue), std::max(m_MinValue, m_MaxValue));
}

void CSliderComponent::Serialize(nlohmann::json& json) const
{
    json["interactable"] = m_Interactable;
    json["raycast_target"] = m_RaycastTarget;
    json["direction"] = static_cast<int>(m_Direction);
    json["min_value"] = m_MinValue;
    json["max_value"] = m_MaxValue;
    json["value"] = m_Value;
    json["whole_numbers"] = m_WholeNumbers;
    json["background_color"] = SerializeGuiColor(m_BackgroundColor);
    json["fill_color"] = SerializeGuiColor(m_FillColor);
    json["handle_color"] = SerializeGuiColor(m_HandleColor);
    json["disabled_color"] = SerializeGuiColor(m_DisabledColor);
    json["on_value_changed_action"] = m_OnValueChangedAction;
}

void CSliderComponent::Deserialize(const nlohmann::json& json)
{
    m_Interactable = json.value("interactable", m_Interactable);
    m_RaycastTarget = json.value("raycast_target", m_RaycastTarget);
    if (json.contains("direction") && json["direction"].is_number_integer())
    {
        const int dir = json["direction"].get<int>();
        if (dir >= static_cast<int>(EUiSliderDirection::LEFT_TO_RIGHT) &&
            dir <= static_cast<int>(EUiSliderDirection::TOP_TO_BOTTOM))
        {
            m_Direction = static_cast<EUiSliderDirection>(dir);
        }
    }
    m_MinValue = json.value("min_value", m_MinValue);
    m_MaxValue = json.value("max_value", m_MaxValue);
    m_Value = json.value("value", m_Value);
    m_WholeNumbers = json.value("whole_numbers", m_WholeNumbers);
    if (m_WholeNumbers)
    {
        m_Value = std::round(m_Value);
    }
    DeserializeGuiColor(json, "background_color", m_BackgroundColor);
    DeserializeGuiColor(json, "fill_color", m_FillColor);
    DeserializeGuiColor(json, "handle_color", m_HandleColor);
    DeserializeGuiColor(json, "disabled_color", m_DisabledColor);
    m_OnValueChangedAction = json.value("on_value_changed_action", m_OnValueChangedAction);
}

void CToggleComponent::Serialize(nlohmann::json& json) const
{
    json["interactable"] = m_Interactable;
    json["raycast_target"] = m_RaycastTarget;
    json["is_on"] = m_IsOn;
    json["label"] = m_Label;
    json["font_size"] = m_FontSize;
    json["box_color"] = SerializeGuiColor(m_BoxColor);
    json["checkmark_color"] = SerializeGuiColor(m_CheckmarkColor);
    json["label_color"] = SerializeGuiColor(m_LabelColor);
    json["disabled_color"] = SerializeGuiColor(m_DisabledColor);
    json["on_value_changed_action"] = m_OnValueChangedAction;
}

void CToggleComponent::Deserialize(const nlohmann::json& json)
{
    m_Interactable = json.value("interactable", m_Interactable);
    m_RaycastTarget = json.value("raycast_target", m_RaycastTarget);
    m_IsOn = json.value("is_on", m_IsOn);
    m_Label = json.value("label", m_Label);
    m_FontSize = std::max(1, json.value("font_size", m_FontSize));
    DeserializeGuiColor(json, "box_color", m_BoxColor);
    DeserializeGuiColor(json, "checkmark_color", m_CheckmarkColor);
    DeserializeGuiColor(json, "label_color", m_LabelColor);
    DeserializeGuiColor(json, "disabled_color", m_DisabledColor);
    m_OnValueChangedAction = json.value("on_value_changed_action", m_OnValueChangedAction);
}

float CProgressBarComponent::GetNormalizedValue() const
{
    const float span = m_MaxValue - m_MinValue;
    if (std::abs(span) <= 1e-6f)
    {
        return 0.0f;
    }
    return std::clamp((m_Value - m_MinValue) / span, 0.0f, 1.0f);
}

void CProgressBarComponent::Serialize(nlohmann::json& json) const
{
    json["raycast_target"] = m_RaycastTarget;
    json["direction"] = static_cast<int>(m_Direction);
    json["min_value"] = m_MinValue;
    json["max_value"] = m_MaxValue;
    json["value"] = m_Value;
    json["show_percentage_text"] = m_ShowPercentageText;
    json["background_color"] = SerializeGuiColor(m_BackgroundColor);
    json["fill_color"] = SerializeGuiColor(m_FillColor);
    json["border_color"] = SerializeGuiColor(m_BorderColor);
    json["text_color"] = SerializeGuiColor(m_TextColor);
}

void CProgressBarComponent::Deserialize(const nlohmann::json& json)
{
    m_RaycastTarget = json.value("raycast_target", m_RaycastTarget);
    if (json.contains("direction") && json["direction"].is_number_integer())
    {
        const int dir = json["direction"].get<int>();
        if (dir >= static_cast<int>(EUiSliderDirection::LEFT_TO_RIGHT) &&
            dir <= static_cast<int>(EUiSliderDirection::TOP_TO_BOTTOM))
        {
            m_Direction = static_cast<EUiSliderDirection>(dir);
        }
    }
    m_MinValue = json.value("min_value", m_MinValue);
    m_MaxValue = json.value("max_value", m_MaxValue);
    m_Value = json.value("value", m_Value);
    m_ShowPercentageText = json.value("show_percentage_text", m_ShowPercentageText);
    DeserializeGuiColor(json, "background_color", m_BackgroundColor);
    DeserializeGuiColor(json, "fill_color", m_FillColor);
    DeserializeGuiColor(json, "border_color", m_BorderColor);
    DeserializeGuiColor(json, "text_color", m_TextColor);
}

void CScrollViewComponent::Serialize(nlohmann::json& json) const
{
    json["raycast_target"] = m_RaycastTarget;
    json["horizontal"] = m_Horizontal;
    json["vertical"] = m_Vertical;
    json["scroll_sensitivity"] = m_ScrollSensitivity;
    json["content_size"] = { m_ContentSize.x, m_ContentSize.y };
    json["normalized_position"] = { m_NormalizedPosition.x, m_NormalizedPosition.y };
    json["scroll_offset"] = { m_ScrollOffset.x, m_ScrollOffset.y };
    json["show_horizontal_scrollbar"] = m_ShowHorizontalScrollbar;
    json["show_vertical_scrollbar"] = m_ShowVerticalScrollbar;
    json["background_color"] = SerializeGuiColor(m_BackgroundColor);
    json["scrollbar_track_color"] = SerializeGuiColor(m_ScrollbarTrackColor);
    json["scrollbar_thumb_color"] = SerializeGuiColor(m_ScrollbarThumbColor);
}

void CScrollViewComponent::Deserialize(const nlohmann::json& json)
{
    m_RaycastTarget = json.value("raycast_target", m_RaycastTarget);
    m_Horizontal = json.value("horizontal", m_Horizontal);
    m_Vertical = json.value("vertical", m_Vertical);
    m_ScrollSensitivity = json.value("scroll_sensitivity", m_ScrollSensitivity);
    if (json.contains("content_size") && json["content_size"].is_array() && json["content_size"].size() >= 2)
    {
        m_ContentSize = { json["content_size"][0].get<float>(), json["content_size"][1].get<float>() };
    }
    if (json.contains("normalized_position") && json["normalized_position"].is_array() && json["normalized_position"].size() >= 2)
    {
        m_NormalizedPosition = {
            std::clamp(json["normalized_position"][0].get<float>(), 0.0f, 1.0f),
            std::clamp(json["normalized_position"][1].get<float>(), 0.0f, 1.0f)
        };
    }
    if (json.contains("scroll_offset") && json["scroll_offset"].is_array() && json["scroll_offset"].size() >= 2)
    {
        m_ScrollOffset = { json["scroll_offset"][0].get<float>(), json["scroll_offset"][1].get<float>() };
    }
    m_ShowHorizontalScrollbar = json.value("show_horizontal_scrollbar", m_ShowHorizontalScrollbar);
    m_ShowVerticalScrollbar = json.value("show_vertical_scrollbar", m_ShowVerticalScrollbar);
    DeserializeGuiColor(json, "background_color", m_BackgroundColor);
    DeserializeGuiColor(json, "scrollbar_track_color", m_ScrollbarTrackColor);
    DeserializeGuiColor(json, "scrollbar_thumb_color", m_ScrollbarThumbColor);
}

void CHorizontalLayoutComponent::Serialize(nlohmann::json& json) const
{
    json["padding"] = SerializeLayoutPadding(m_Padding);
    json["spacing"] = m_Spacing;
    json["child_alignment"] = static_cast<int>(m_ChildAlignment);
    json["reverse_arrangement"] = m_ReverseArrangement;
    json["child_control_width"] = m_ChildControlWidth;
    json["child_control_height"] = m_ChildControlHeight;
    json["child_scale_width"] = m_ChildScaleWidth;
    json["child_scale_height"] = m_ChildScaleHeight;
    json["child_force_expand_width"] = m_ChildForceExpandWidth;
    json["child_force_expand_height"] = m_ChildForceExpandHeight;
}

void CHorizontalLayoutComponent::Deserialize(const nlohmann::json& json)
{
    DeserializeLayoutPadding(json, "padding", m_Padding);
    m_Spacing = json.value("spacing", m_Spacing);
    if (json.contains("child_alignment") && json["child_alignment"].is_number_integer())
    {
        const int align = json["child_alignment"].get<int>();
        if (align >= static_cast<int>(EUiLayoutChildAlignment::UPPER_LEFT) &&
            align <= static_cast<int>(EUiLayoutChildAlignment::LOWER_RIGHT))
        {
            m_ChildAlignment = static_cast<EUiLayoutChildAlignment>(align);
        }
    }
    m_ReverseArrangement = json.value("reverse_arrangement", m_ReverseArrangement);
    m_ChildControlWidth = json.value("child_control_width", m_ChildControlWidth);
    m_ChildControlHeight = json.value("child_control_height", m_ChildControlHeight);
    m_ChildScaleWidth = json.value("child_scale_width", m_ChildScaleWidth);
    m_ChildScaleHeight = json.value("child_scale_height", m_ChildScaleHeight);
    m_ChildForceExpandWidth = json.value("child_force_expand_width", m_ChildForceExpandWidth);
    m_ChildForceExpandHeight = json.value("child_force_expand_height", m_ChildForceExpandHeight);
}

void CVerticalLayoutComponent::Serialize(nlohmann::json& json) const
{
    json["padding"] = SerializeLayoutPadding(m_Padding);
    json["spacing"] = m_Spacing;
    json["child_alignment"] = static_cast<int>(m_ChildAlignment);
    json["reverse_arrangement"] = m_ReverseArrangement;
    json["child_control_width"] = m_ChildControlWidth;
    json["child_control_height"] = m_ChildControlHeight;
    json["child_scale_width"] = m_ChildScaleWidth;
    json["child_scale_height"] = m_ChildScaleHeight;
    json["child_force_expand_width"] = m_ChildForceExpandWidth;
    json["child_force_expand_height"] = m_ChildForceExpandHeight;
}

void CVerticalLayoutComponent::Deserialize(const nlohmann::json& json)
{
    DeserializeLayoutPadding(json, "padding", m_Padding);
    m_Spacing = json.value("spacing", m_Spacing);
    if (json.contains("child_alignment") && json["child_alignment"].is_number_integer())
    {
        const int align = json["child_alignment"].get<int>();
        if (align >= static_cast<int>(EUiLayoutChildAlignment::UPPER_LEFT) &&
            align <= static_cast<int>(EUiLayoutChildAlignment::LOWER_RIGHT))
        {
            m_ChildAlignment = static_cast<EUiLayoutChildAlignment>(align);
        }
    }
    m_ReverseArrangement = json.value("reverse_arrangement", m_ReverseArrangement);
    m_ChildControlWidth = json.value("child_control_width", m_ChildControlWidth);
    m_ChildControlHeight = json.value("child_control_height", m_ChildControlHeight);
    m_ChildScaleWidth = json.value("child_scale_width", m_ChildScaleWidth);
    m_ChildScaleHeight = json.value("child_scale_height", m_ChildScaleHeight);
    m_ChildForceExpandWidth = json.value("child_force_expand_width", m_ChildForceExpandWidth);
    m_ChildForceExpandHeight = json.value("child_force_expand_height", m_ChildForceExpandHeight);
}

void CGridLayoutComponent::Serialize(nlohmann::json& json) const
{
    json["padding"] = SerializeLayoutPadding(m_Padding);
    json["cell_size"] = { m_CellSize.x, m_CellSize.y };
    json["spacing"] = { m_Spacing.x, m_Spacing.y };
    json["start_corner"] = static_cast<int>(m_StartCorner);
    json["start_axis"] = static_cast<int>(m_StartAxis);
    json["child_alignment"] = static_cast<int>(m_ChildAlignment);
    json["constraint"] = static_cast<int>(m_Constraint);
    json["constraint_count"] = m_ConstraintCount;
}

void CGridLayoutComponent::Deserialize(const nlohmann::json& json)
{
    DeserializeLayoutPadding(json, "padding", m_Padding);
    if (json.contains("cell_size") && json["cell_size"].is_array() && json["cell_size"].size() >= 2)
    {
        m_CellSize = {
            std::max(1.0f, json["cell_size"][0].get<float>()),
            std::max(1.0f, json["cell_size"][1].get<float>())
        };
    }
    if (json.contains("spacing") && json["spacing"].is_array() && json["spacing"].size() >= 2)
    {
        m_Spacing = { json["spacing"][0].get<float>(), json["spacing"][1].get<float>() };
    }
    if (json.contains("start_corner") && json["start_corner"].is_number_integer())
    {
        const int corner = json["start_corner"].get<int>();
        if (corner >= static_cast<int>(EUiGridStartCorner::UPPER_LEFT) &&
            corner <= static_cast<int>(EUiGridStartCorner::LOWER_RIGHT))
        {
            m_StartCorner = static_cast<EUiGridStartCorner>(corner);
        }
    }
    if (json.contains("start_axis") && json["start_axis"].is_number_integer())
    {
        const int axis = json["start_axis"].get<int>();
        if (axis >= static_cast<int>(EUiGridStartAxis::HORIZONTAL) &&
            axis <= static_cast<int>(EUiGridStartAxis::VERTICAL))
        {
            m_StartAxis = static_cast<EUiGridStartAxis>(axis);
        }
    }
    if (json.contains("child_alignment") && json["child_alignment"].is_number_integer())
    {
        const int align = json["child_alignment"].get<int>();
        if (align >= static_cast<int>(EUiLayoutChildAlignment::UPPER_LEFT) &&
            align <= static_cast<int>(EUiLayoutChildAlignment::LOWER_RIGHT))
        {
            m_ChildAlignment = static_cast<EUiLayoutChildAlignment>(align);
        }
    }
    if (json.contains("constraint") && json["constraint"].is_number_integer())
    {
        const int constraint = json["constraint"].get<int>();
        if (constraint >= static_cast<int>(EUiGridConstraint::FLEXIBLE) &&
            constraint <= static_cast<int>(EUiGridConstraint::FIXED_ROW_COUNT))
        {
            m_Constraint = static_cast<EUiGridConstraint>(constraint);
        }
    }
    m_ConstraintCount = std::max(1, json.value("constraint_count", m_ConstraintCount));
}

void CComponentManager::Deserialize(const nlohmann::json& json, const CComponentFactoryRegistry& factories)
{
    if (!json.contains("components"))
    {
        return;
    }

    m_vComponents.clear();
    m_apFastLookup.fill(nullptr);
    nlohmann::json legacyUiData;
    for (const auto& compJson : json["components"])
    {
        const std::string typeName = compJson["type"];
        const bool compEnabled = compJson.value("enabled", true);

        std::shared_ptr<IComponent> pComp = factories.Create(typeName);
        if (typeName == "Text" && compJson.contains("data") &&
            compJson["data"].contains("thickness"))
        {
            pComp = std::make_shared<CText3DComponent>();
        }
        if (!pComp)
        {
            TraceLog(LogLevel::Warn, "COMPONENT",
                     TextFormat("Dropping component '%s': no factory is registered for this type name.",
                                typeName.c_str()));
            continue;
        }

        if (compJson.contains("data"))
        {
            if (pComp->GetType() == COMPONENT_UI)
            {
                legacyUiData = compJson["data"];
            }
            pComp->Deserialize(compJson["data"]);
        }
        pComp->m_Enabled = compEnabled;
        m_vComponents.push_back(pComp);
    }

    RebuildFastLookup();

    if (CUiComponent* pUi = GetUiComponent())
    {
        CUiRectTransformComponent* pRectTransform = GetRectTransform();
        if (!pRectTransform)
        {
            auto pNewRectTransform = std::make_shared<CUiRectTransformComponent>();
            if (legacyUiData.contains("position") && legacyUiData["position"].is_array() &&
                legacyUiData["position"].size() >= 2)
            {
                pNewRectTransform->m_Position = {
                    legacyUiData["position"][0].get<float>(),
                    legacyUiData["position"][1].get<float>()
                };
            }
            if (legacyUiData.contains("size") && legacyUiData["size"].is_array() &&
                legacyUiData["size"].size() >= 2)
            {
                pNewRectTransform->m_Size = {
                    legacyUiData["size"][0].get<float>(),
                    legacyUiData["size"][1].get<float>()
                };
            }
            m_vComponents.insert(m_vComponents.begin(), pNewRectTransform);
        }

        CTextComponent* pText = GetText();
        if (!pText)
        {
            auto pNewText = std::make_shared<CTextComponent>();
            if (legacyUiData.contains("text") && legacyUiData["text"].is_string())
            {
                pNewText->m_Text = legacyUiData["text"].get<std::string>();
            }
            else if (pUi->m_WidgetType == EGuiWidgetType::BUTTON)
            {
                pNewText->m_Text = "Button";
            }
            else if (pUi->m_WidgetType == EGuiWidgetType::INPUT_FIELD)
            {
                pNewText->m_Text.clear();
            }
            else
            {
                pNewText->m_Text = "Combo";
            }
            pNewText->m_HorizontalAlignment = pUi->m_WidgetType == EGuiWidgetType::BUTTON
                ? EUiTextAlignment::CENTER
                : EUiTextAlignment::LEFT;
            m_vComponents.push_back(pNewText);
        }

        RemoveMeshAndMaterialForUi();
    }
}

CScriptComponent* CComponentManager::GetScript()
{
    return GetFastComponent<CScriptComponent>(COMPONENT_SCRIPT);
}

void CComponentManager::CloneInto(const CComponentManager& source)
{
    m_vComponents.clear();
    m_apFastLookup.fill(nullptr);
    m_vComponents.reserve(source.m_vComponents.size());

    for (const auto& pComponent : source.m_vComponents)
    {
        if (!pComponent)
        {
            continue;
        }

        std::shared_ptr<IComponent> pCloned;
        switch (pComponent->GetType())
        {
            case COMPONENT_TRANSFORM:
                pCloned = std::make_shared<CTransformComponent>(
                    *static_cast<const CTransformComponent*>(pComponent.get()));
                break;

            case COMPONENT_MESH:
                pCloned = std::make_shared<CMeshComponent>(
                    *static_cast<const CMeshComponent*>(pComponent.get()));
                break;

            case COMPONENT_LIGHT:
                pCloned = std::make_shared<CLightComponent>(
                    *static_cast<const CLightComponent*>(pComponent.get()));
                break;

            case COMPONENT_MATERIAL:
                pCloned = std::make_shared<CMaterialComponent>(
                    *static_cast<const CMaterialComponent*>(pComponent.get()));
                break;

            case COMPONENT_COLLISION:
                pCloned = std::make_shared<CCollisionComponent>(
                    *static_cast<const CCollisionComponent*>(pComponent.get()));
                break;

            case COMPONENT_RIGIDBODY:
                pCloned = std::make_shared<CRigidbodyComponent>(
                    *static_cast<const CRigidbodyComponent*>(pComponent.get()));
                break;

            case COMPONENT_CAMERA:
                pCloned = std::make_shared<CCameraComponent>(
                    *static_cast<const CCameraComponent*>(pComponent.get()));
                break;

            case COMPONENT_SCRIPT:
                pCloned = std::make_shared<CScriptComponent>(
                    *static_cast<const CScriptComponent*>(pComponent.get()));
                break;

            case COMPONENT_UI:
                pCloned = std::make_shared<CUiComponent>(
                    *static_cast<const CUiComponent*>(pComponent.get()));
                break;

            case COMPONENT_TEXT:
                pCloned = std::make_shared<CTextComponent>(
                    *static_cast<const CTextComponent*>(pComponent.get()));
                break;

            case COMPONENT_UI_RECT_TRANSFORM:
                pCloned = std::make_shared<CUiRectTransformComponent>(
                    *static_cast<const CUiRectTransformComponent*>(pComponent.get()));
                break;

            case COMPONENT_CANVAS:
                pCloned = std::make_shared<CCanvasComponent>(
                    *static_cast<const CCanvasComponent*>(pComponent.get()));
                break;

            case COMPONENT_IMAGE:
                pCloned = std::make_shared<CImageComponent>(
                    *static_cast<const CImageComponent*>(pComponent.get()));
                break;

            case COMPONENT_PANEL:
                pCloned = std::make_shared<CPanelComponent>(
                    *static_cast<const CPanelComponent*>(pComponent.get()));
                break;

            case COMPONENT_BUTTON:
                pCloned = std::make_shared<CButtonComponent>(
                    *static_cast<const CButtonComponent*>(pComponent.get()));
                break;

            case COMPONENT_SLIDER:
                pCloned = std::make_shared<CSliderComponent>(
                    *static_cast<const CSliderComponent*>(pComponent.get()));
                break;

            case COMPONENT_TOGGLE:
                pCloned = std::make_shared<CToggleComponent>(
                    *static_cast<const CToggleComponent*>(pComponent.get()));
                break;

            case COMPONENT_PROGRESS_BAR:
                pCloned = std::make_shared<CProgressBarComponent>(
                    *static_cast<const CProgressBarComponent*>(pComponent.get()));
                break;

            case COMPONENT_SCROLL_VIEW:
                pCloned = std::make_shared<CScrollViewComponent>(
                    *static_cast<const CScrollViewComponent*>(pComponent.get()));
                break;

            case COMPONENT_HORIZONTAL_LAYOUT:
                pCloned = std::make_shared<CHorizontalLayoutComponent>(
                    *static_cast<const CHorizontalLayoutComponent*>(pComponent.get()));
                break;

            case COMPONENT_VERTICAL_LAYOUT:
                pCloned = std::make_shared<CVerticalLayoutComponent>(
                    *static_cast<const CVerticalLayoutComponent*>(pComponent.get()));
                break;

            case COMPONENT_GRID_LAYOUT:
                pCloned = std::make_shared<CGridLayoutComponent>(
                    *static_cast<const CGridLayoutComponent*>(pComponent.get()));
                break;

            default:
                break;
        }

        if (pCloned)
        {
            AddComponent(std::move(pCloned));
        }
    }
}
