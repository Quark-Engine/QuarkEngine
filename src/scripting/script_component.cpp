#include "scripting/script_component.h"
#include "entity.h"

namespace quark::script
{

SScriptTransform CScriptEntity::GetFallbackTransform()
{
    CTransformComponent* pTransform = GetTransformComponent();
    if (pTransform)
    {
        pTransform->ClearLocalMatrixOverride();
        m_pPosition = &pTransform->m_Position;
        m_pRotation = &pTransform->m_Rotation;
        m_pScale = &pTransform->m_Scale;
        m_pHasLocalMatrixOverride = &pTransform->m_HasLocalMatrixOverride;
        return SScriptTransform{ *m_pPosition, *m_pRotation, *m_pScale };
    }
    static Vec3 s_DummyPos{0.0f, 0.0f, 0.0f};
    static Vec3 s_DummyRot{0.0f, 0.0f, 0.0f};
    static Vec3 s_DummyScale{1.0f, 1.0f, 1.0f};
    return SScriptTransform{ s_DummyPos, s_DummyRot, s_DummyScale };
}

int CScriptEntity::GetId() const
{
    return m_pEntity ? m_pEntity->m_Id : -1;
}

std::string CScriptEntity::GetName() const
{
    return m_pEntity ? m_pEntity->m_Name : "";
}

CTransformComponent* CScriptEntity::GetTransformComponent()
{
    return m_pEntity ? m_pEntity->GetTransformComponent() : nullptr;
}

CMeshComponent* CScriptEntity::GetMeshComponent()
{
    return m_pEntity ? m_pEntity->GetMeshComponent() : nullptr;
}

CMaterialComponent* CScriptEntity::GetMaterialComponent()
{
    return m_pEntity ? m_pEntity->GetMaterialComponent() : nullptr;
}

CLightComponent* CScriptEntity::GetLightComponent()
{
    return m_pEntity ? m_pEntity->GetLightComponent() : nullptr;
}

CCameraComponent* CScriptEntity::GetCameraComponent()
{
    return m_pEntity ? m_pEntity->GetCameraComponent() : nullptr;
}

CCollisionComponent* CScriptEntity::GetCollisionComponent()
{
    return m_pEntity ? m_pEntity->GetCollisionComponent() : nullptr;
}

CScriptComponent::CScriptComponent()
    : IComponent(COMPONENT_SCRIPT, "Script")
{
}

CScriptComponent::~CScriptComponent() = default;

CScriptComponent::CScriptComponent(const CScriptComponent& other)
    : IComponent(other),
      m_ScriptName(other.m_ScriptName),
      m_ScriptPath(other.m_ScriptPath),
      m_vFields(other.m_vFields),
      m_pInstance(nullptr),
      m_pScriptEntity(nullptr)
{
}

CScriptComponent& CScriptComponent::operator=(const CScriptComponent& other)
{
    if (this == &other)
    {
        return *this;
    }

    IComponent::operator=(other);
    m_ScriptName = other.m_ScriptName;
    m_ScriptPath = other.m_ScriptPath;
    m_vFields = other.m_vFields;
    m_pInstance = nullptr;
    m_pScriptEntity = nullptr;
    return *this;
}

EComponentType CScriptComponent::GetType() const
{
    return COMPONENT_SCRIPT;
}

std::string CScriptComponent::GetTypeName() const
{
    return "Script";
}

void CScriptComponent::Serialize(nlohmann::json& json) const
{
    json["script_name"] = m_ScriptName;
    json["script_path"] = m_ScriptPath;

    nlohmann::json fieldsArray = nlohmann::json::array();
    for (const auto& field : m_vFields)
    {
        nlohmann::json fieldJson;
        fieldJson["name"] = field.name;
        fieldJson["type"] = static_cast<int>(field.type);
        switch (field.type)
        {
        case EScriptFieldType::FLOAT:
            fieldJson["value"] = field.floatValue;
            break;
        case EScriptFieldType::INT:
            fieldJson["value"] = field.intValue;
            break;
        case EScriptFieldType::BOOL:
            fieldJson["value"] = field.boolValue;
            break;
        case EScriptFieldType::STRING:
            fieldJson["value"] = field.stringValue;
            break;
        case EScriptFieldType::VEC3:
            fieldJson["value"] = {field.vec3Value.x, field.vec3Value.y, field.vec3Value.z};
            break;
        }
        fieldsArray.push_back(fieldJson);
    }
    json["fields"] = fieldsArray;
}

void CScriptComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("script_name") && json["script_name"].is_string())
    {
        m_ScriptName = json["script_name"].get<std::string>();
    }
    if (json.contains("script_path") && json["script_path"].is_string())
    {
        m_ScriptPath = json["script_path"].get<std::string>();
    }

    if (json.contains("fields") && json["fields"].is_array())
    {
        m_vFields.clear();
        for (const auto& fieldJson : json["fields"])
        {
            if (!fieldJson.contains("name") || !fieldJson.contains("type"))
            {
                continue;
            }

            SScriptField field;
            field.name = fieldJson["name"].get<std::string>();
            field.type = static_cast<EScriptFieldType>(fieldJson["type"].get<int>());

            if (fieldJson.contains("value"))
            {
                const auto& val = fieldJson["value"];
                switch (field.type)
                {
                case EScriptFieldType::FLOAT:
                    if (val.is_number())
                    {
                        field.floatValue = val.get<float>();
                    }
                    break;
                case EScriptFieldType::INT:
                    if (val.is_number())
                    {
                        field.intValue = val.get<int>();
                    }
                    break;
                case EScriptFieldType::BOOL:
                    if (val.is_boolean())
                    {
                        field.boolValue = val.get<bool>();
                    }
                    break;
                case EScriptFieldType::STRING:
                    if (val.is_string())
                    {
                        field.stringValue = val.get<std::string>();
                    }
                    break;
                case EScriptFieldType::VEC3:
                    if (val.is_array() && val.size() >= 3)
                    {
                        field.vec3Value = {val[0].get<float>(), val[1].get<float>(), val[2].get<float>()};
                    }
                    break;
                }
            }
            m_vFields.push_back(field);
        }
    }
}

SScriptField* CScriptComponent::FindField(const std::string& name)
{
    for (auto& field : m_vFields)
    {
        if (field.name == name)
        {
            return &field;
        }
    }
    return nullptr;
}

const SScriptField* CScriptComponent::FindField(const std::string& name) const
{
    for (const auto& field : m_vFields)
    {
        if (field.name == name)
        {
            return &field;
        }
    }
    return nullptr;
}

void CScriptComponent::SetFieldValue(const std::string& name, float value)
{
    SScriptField* pField = FindField(name);
    if (pField)
    {
        pField->floatValue = value;
    }
}

void CScriptComponent::SetFieldValue(const std::string& name, int value)
{
    SScriptField* pField = FindField(name);
    if (pField)
    {
        pField->intValue = value;
    }
}

void CScriptComponent::SetFieldValue(const std::string& name, bool value)
{
    SScriptField* pField = FindField(name);
    if (pField)
    {
        pField->boolValue = value;
    }
}

void CScriptComponent::SetFieldValue(const std::string& name, const std::string& value)
{
    SScriptField* pField = FindField(name);
    if (pField)
    {
        pField->stringValue = value;
    }
}

void CScriptComponent::SetFieldValue(const std::string& name, const Vec3& value)
{
    SScriptField* pField = FindField(name);
    if (pField)
    {
        pField->vec3Value = value;
    }
}

void CScriptComponent::SyncWithHeaderFields(const std::vector<SScriptField>& vHeaderFields)
{
    std::vector<SScriptField> vUpdated;
    vUpdated.reserve(vHeaderFields.size());

    for (const auto& headerField : vHeaderFields)
    {
        const SScriptField* pExisting = FindField(headerField.name);
        if (pExisting && pExisting->type == headerField.type)
        {
            vUpdated.push_back(*pExisting);
        }
        else
        {
            vUpdated.push_back(headerField);
        }
    }

    m_vFields = std::move(vUpdated);
}

void CScriptComponent::ApplyFieldsToInstance()
{
    if (!m_pInstance)
    {
        return;
    }

    size_t currentOffset = sizeof(IScript);

    for (const auto& field : m_vFields)
    {
        switch (field.type)
        {
        case EScriptFieldType::FLOAT:
        {
            currentOffset = (currentOffset + 3) & ~size_t(3);
            auto* pDst = reinterpret_cast<float*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
            *pDst = field.floatValue;
            m_pInstance->SetPropertyFloat(field.name, field.floatValue);
            currentOffset += sizeof(float);
            break;
        }
        case EScriptFieldType::INT:
        {
            currentOffset = (currentOffset + 3) & ~size_t(3);
            auto* pDst = reinterpret_cast<int*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
            *pDst = field.intValue;
            m_pInstance->SetPropertyInt(field.name, field.intValue);
            currentOffset += sizeof(int);
            break;
        }
        case EScriptFieldType::BOOL:
        {
            auto* pDst = reinterpret_cast<bool*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
            *pDst = field.boolValue;
            m_pInstance->SetPropertyBool(field.name, field.boolValue);
            currentOffset += sizeof(bool);
            break;
        }
        case EScriptFieldType::VEC3:
        {
            currentOffset = (currentOffset + 3) & ~size_t(3);
            auto* pDst = reinterpret_cast<Vec3*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
            *pDst = field.vec3Value;
            m_pInstance->SetPropertyVec3(field.name, field.vec3Value);
            currentOffset += sizeof(Vec3);
            break;
        }
        case EScriptFieldType::STRING:
        {
            currentOffset = (currentOffset + alignof(std::string) - 1) & ~(alignof(std::string) - 1);
            m_pInstance->SetPropertyString(field.name, field.stringValue);
            currentOffset += sizeof(std::string);
            break;
        }
        }
    }
}

void CScriptComponent::ReadFieldsFromInstance()
{
    if (!m_pInstance)
    {
        return;
    }

    size_t currentOffset = sizeof(IScript);

    for (auto& field : m_vFields)
    {
        switch (field.type)
        {
        case EScriptFieldType::FLOAT:
        {
            currentOffset = (currentOffset + 3) & ~size_t(3);
            float val = 0.0f;
            if (m_pInstance->GetPropertyFloat(field.name, val))
            {
                field.floatValue = val;
            }
            else
            {
                auto* pSrc = reinterpret_cast<const float*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                field.floatValue = *pSrc;
            }
            currentOffset += sizeof(float);
            break;
        }
        case EScriptFieldType::INT:
        {
            currentOffset = (currentOffset + 3) & ~size_t(3);
            int val = 0;
            if (m_pInstance->GetPropertyInt(field.name, val))
            {
                field.intValue = val;
            }
            else
            {
                auto* pSrc = reinterpret_cast<const int*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                field.intValue = *pSrc;
            }
            currentOffset += sizeof(int);
            break;
        }
        case EScriptFieldType::BOOL:
        {
            bool val = false;
            if (m_pInstance->GetPropertyBool(field.name, val))
            {
                field.boolValue = val;
            }
            else
            {
                auto* pSrc = reinterpret_cast<const bool*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                field.boolValue = *pSrc;
            }
            currentOffset += sizeof(bool);
            break;
        }
        case EScriptFieldType::VEC3:
        {
            currentOffset = (currentOffset + 3) & ~size_t(3);
            Vec3 val{};
            if (m_pInstance->GetPropertyVec3(field.name, val))
            {
                field.vec3Value = val;
            }
            else
            {
                auto* pSrc = reinterpret_cast<const Vec3*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                field.vec3Value = *pSrc;
            }
            currentOffset += sizeof(Vec3);
            break;
        }
        case EScriptFieldType::STRING:
        {
            currentOffset = (currentOffset + alignof(std::string) - 1) & ~(alignof(std::string) - 1);
            std::string val;
            if (m_pInstance->GetPropertyString(field.name, val))
            {
                field.stringValue = val;
            }
            currentOffset += sizeof(std::string);
            break;
        }
        }
    }
}

} // quark::script
