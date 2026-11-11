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
    json["rich_text"] = m_RichText;
    json["horizontal_alignment"] = static_cast<int>(m_HorizontalAlignment);
    json["vertical_alignment"] = static_cast<int>(m_VerticalAlignment);
    json["horizontal_overflow"] = static_cast<int>(m_HorizontalOverflow);
    json["vertical_overflow"] = static_cast<int>(m_VerticalOverflow);
    json["best_fit"] = m_BestFit;
    json["color"] = SerializeGuiColor(m_Color);
    json["material"] = m_Material;
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

            default:
                break;
        }

        if (pCloned)
        {
            m_vComponents.push_back(pCloned);
        }
    }
}
