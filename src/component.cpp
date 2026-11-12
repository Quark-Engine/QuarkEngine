#include "component.h"
#include "scripting/script_component.h"
#define _CRT_SECURE_NO_WARNINGS

#include "engine/component_factory_registry.h"
#include "entity.h"
#include "nlohmann/json.hpp"

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
    json["color_multiplier"] = m_ColorMultiplier;
    json["fade_duration"] = m_FadeDuration;
    json["sprite_swap_highlighted"] = m_SpriteSwapHighlighted;
    json["sprite_swap_pressed"] = m_SpriteSwapPressed;
    json["sprite_swap_selected"] = m_SpriteSwapSelected;
    json["sprite_swap_disabled"] = m_SpriteSwapDisabled;
    json["on_click_action"] = m_OnClickAction;
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
    if (json.contains("raycast_target") && json["raycast_target"].is_boolean())
    {
        m_RaycastTarget = json["raycast_target"].get<bool>();
    }
}

void CComponentManager::Deserialize(const nlohmann::json& json, const CComponentFactoryRegistry& factories)
{
    if (!json.contains("components"))
    {
        return;
    }

    m_vComponents.clear();
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
            if (std::dynamic_pointer_cast<CUiComponent>(pComp))
            {
                legacyUiData = compJson["data"];
            }
            pComp->Deserialize(compJson["data"]);
        }
        pComp->m_Enabled = compEnabled;
        m_vComponents.push_back(pComp);
    }

    if (auto pUi = GetComponentOfType<CUiComponent>())
    {
        auto pRectTransform = GetComponentOfType<CUiRectTransformComponent>();
        if (!pRectTransform)
        {
            pRectTransform = std::make_shared<CUiRectTransformComponent>();
            if (legacyUiData.contains("position") && legacyUiData["position"].is_array() &&
                legacyUiData["position"].size() >= 2)
            {
                pRectTransform->m_Position = {
                    legacyUiData["position"][0].get<float>(),
                    legacyUiData["position"][1].get<float>()
                };
            }
            if (legacyUiData.contains("size") && legacyUiData["size"].is_array() &&
                legacyUiData["size"].size() >= 2)
            {
                pRectTransform->m_Size = {
                    legacyUiData["size"][0].get<float>(),
                    legacyUiData["size"][1].get<float>()
                };
            }
            m_vComponents.insert(m_vComponents.begin(), pRectTransform);
        }

        auto pText = GetComponentOfType<CTextComponent>();
        if (!pText)
        {
            pText = std::make_shared<CTextComponent>();
            if (legacyUiData.contains("text") && legacyUiData["text"].is_string())
            {
                pText->m_Text = legacyUiData["text"].get<std::string>();
            }
            else if (pUi->m_WidgetType == EGuiWidgetType::BUTTON)
            {
                pText->m_Text = "Button";
            }
            else if (pUi->m_WidgetType == EGuiWidgetType::INPUT_FIELD)
            {
                pText->m_Text.clear();
            }
            else
            {
                pText->m_Text = "Combo";
            }
            pText->m_HorizontalAlignment = pUi->m_WidgetType == EGuiWidgetType::BUTTON
                ? EUiTextAlignment::CENTER
                : EUiTextAlignment::LEFT;
            m_vComponents.push_back(pText);
        }

        RemoveMeshAndMaterialForUi();
    }
}

CScriptComponent* CComponentManager::GetScript()
{
    auto pComp = GetComponentOfType<CScriptComponent>();
    return pComp ? pComp.get() : nullptr;
}

void CComponentManager::CloneInto(const CComponentManager& source)
{
    m_vComponents.clear();
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
                    *std::dynamic_pointer_cast<CTransformComponent>(pComponent));
                break;

            case COMPONENT_MESH:
                pCloned = std::make_shared<CMeshComponent>(
                    *std::dynamic_pointer_cast<CMeshComponent>(pComponent));
                break;

            case COMPONENT_LIGHT:
                pCloned = std::make_shared<CLightComponent>(
                    *std::dynamic_pointer_cast<CLightComponent>(pComponent));
                break;

            case COMPONENT_MATERIAL:
                pCloned = std::make_shared<CMaterialComponent>(
                    *std::dynamic_pointer_cast<CMaterialComponent>(pComponent));
                break;

            case COMPONENT_COLLISION:
                pCloned = std::make_shared<CCollisionComponent>(
                    *std::dynamic_pointer_cast<CCollisionComponent>(pComponent));
                break;

            case COMPONENT_CAMERA:
                pCloned = std::make_shared<CCameraComponent>(
                    *std::dynamic_pointer_cast<CCameraComponent>(pComponent));
                break;

            case COMPONENT_SCRIPT:
                pCloned = std::make_shared<CScriptComponent>(
                    *std::dynamic_pointer_cast<CScriptComponent>(pComponent));
                break;

            case COMPONENT_UI:
                pCloned = std::make_shared<CUiComponent>(
                    *std::dynamic_pointer_cast<CUiComponent>(pComponent));
                break;

            case COMPONENT_TEXT:
                pCloned = std::make_shared<CTextComponent>(
                    *std::dynamic_pointer_cast<CTextComponent>(pComponent));
                break;

            case COMPONENT_UI_RECT_TRANSFORM:
                pCloned = std::make_shared<CUiRectTransformComponent>(
                    *std::dynamic_pointer_cast<CUiRectTransformComponent>(pComponent));
                break;

            case COMPONENT_CANVAS:
                pCloned = std::make_shared<CCanvasComponent>(
                    *std::dynamic_pointer_cast<CCanvasComponent>(pComponent));
                break;

            case COMPONENT_IMAGE:
                pCloned = std::make_shared<CImageComponent>(
                    *std::dynamic_pointer_cast<CImageComponent>(pComponent));
                break;

            case COMPONENT_PANEL:
                pCloned = std::make_shared<CPanelComponent>(
                    *std::dynamic_pointer_cast<CPanelComponent>(pComponent));
                break;

            case COMPONENT_BUTTON:
                pCloned = std::make_shared<CButtonComponent>(
                    *std::dynamic_pointer_cast<CButtonComponent>(pComponent));
                break;

            default:
                break;
        }

        if (pCloned)
        {
            m_vComponents.push_back(pCloned);
        }
    }
}
