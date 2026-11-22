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
        static Vec3 s_DummyPos{ 0.0f, 0.0f, 0.0f };
        static Vec3 s_DummyRot{ 0.0f, 0.0f, 0.0f };
        static Vec3 s_DummyScale{ 1.0f, 1.0f, 1.0f };
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

    void CScriptEntity::SetName(const std::string& name)
    {
        if (m_pEntity)
        {
            m_pEntity->m_Name = name;
        }
    }

    bool CScriptEntity::HasTag(const std::string& tag) const
    {
        if (!m_pEntity)
        {
            return false;
        }
        for (const std::string& existing : m_pEntity->m_vTags)
        {
            if (existing == tag)
            {
                return true;
            }
        }
        return false;
    }

    void CScriptEntity::AddTag(const std::string& tag)
    {
        if (m_pEntity && !tag.empty() && !HasTag(tag))
        {
            m_pEntity->m_vTags.push_back(tag);
        }
    }

    Color CScriptEntity::GetMaterialColor() const
    {
        if (m_pEntity)
        {
            if (const CMaterialComponent* pMat = m_pEntity->GetMaterialComponent())
            {
                return pMat->m_Color;
            }
        }
        return WHITE;
    }

    void CScriptEntity::SetMaterialColor(const Color& color)
    {
        if (!m_pEntity)
        {
            return;
        }
        CMaterialComponent* pMat = m_pEntity->GetMaterialComponent();
        if (!pMat && m_pEntity->GetComponents())
        {
            m_pEntity->GetComponents()->AddComponent(std::make_shared<CMaterialComponent>());
            pMat = m_pEntity->GetMaterialComponent();
        }
        if (pMat)
        {
            pMat->m_Color = color;
        }
        if (CMeshComponent* pMesh = m_pEntity->GetMeshComponent())
        {
            for (int i = 0; i < pMesh->m_Model.materialCount; ++i)
            {
                if (pMesh->m_Model.materials && pMesh->m_Model.materials[i].maps)
                {
                    pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = color;
                }
            }
        }
    }

    Color CScriptEntity::GetLightColor() const
    {
        if (m_pEntity)
        {
            if (const CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                return pLight->m_Light.m_Color;
            }
        }
        return WHITE;
    }

    void CScriptEntity::SetLightColor(const Color& color)
    {
        if (m_pEntity)
        {
            if (CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                pLight->m_Light.m_Color = color;
                pLight->m_Light.m_Light.color = color;
            }
        }
    }

    float CScriptEntity::GetLightIntensity() const
    {
        if (m_pEntity)
        {
            if (const CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                return pLight->m_Light.m_Intensity;
            }
        }
        return 1.0f;
    }

    void CScriptEntity::SetLightIntensity(float intensity)
    {
        if (m_pEntity)
        {
            if (CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                pLight->m_Light.m_Intensity = intensity;
            }
        }
    }

    float CScriptEntity::GetLightRange() const
    {
        if (m_pEntity)
        {
            if (const CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                return pLight->m_Light.m_Range;
            }
        }
        return 10.0f;
    }

    void CScriptEntity::SetLightRange(float range)
    {
        if (m_pEntity)
        {
            if (CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                pLight->m_Light.m_Range = range;
            }
        }
    }

    bool CScriptEntity::IsLightEnabled() const
    {
        if (m_pEntity)
        {
            if (const CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                return pLight->m_Enabled && pLight->m_Light.m_Enabled;
            }
        }
        return false;
    }

    void CScriptEntity::SetLightEnabled(bool enabled)
    {
        if (m_pEntity)
        {
            if (CLightComponent* pLight = m_pEntity->GetLightComponent())
            {
                pLight->m_Enabled = enabled;
                pLight->m_Light.m_Enabled = enabled;
                pLight->m_Light.m_Light.enabled = enabled ? 1 : 0;
            }
        }
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

    CRigidbodyComponent* CScriptEntity::GetRigidbodyComponent()
    {
        return m_pEntity ? m_pEntity->GetRigidbodyComponent() : nullptr;
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
            fieldJson["name"] = field.m_Name;
            fieldJson["type"] = static_cast<int>(field.m_Type);
            switch (field.m_Type)
            {
                case EScriptFieldType::FLOAT:
                    fieldJson["value"] = field.m_FloatValue;
                    break;
                case EScriptFieldType::INT:
                    fieldJson["value"] = field.m_IntValue;
                    break;
                case EScriptFieldType::BOOL:
                    fieldJson["value"] = field.m_BoolValue;
                    break;
                case EScriptFieldType::STRING:
                    fieldJson["value"] = field.m_StringValue;
                    break;
                case EScriptFieldType::VEC3:
                    fieldJson["value"] = { field.m_Vec3Value.x, field.m_Vec3Value.y, field.m_Vec3Value.z };
                    break;
                case EScriptFieldType::COLOR:
                    fieldJson["value"] = {
                        field.m_ColorValue.r,
                        field.m_ColorValue.g,
                        field.m_ColorValue.b,
                        field.m_ColorValue.a
                    };
                    break;
                case EScriptFieldType::ENTITY_REF:
                    fieldJson["value"] = field.m_EntityIdValue;
                    break;
                case EScriptFieldType::ASSET_REF:
                    fieldJson["value"] = field.m_AssetPathValue;
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
                field.m_Name = fieldJson["name"].get<std::string>();
                field.m_Type = static_cast<EScriptFieldType>(fieldJson["type"].get<int>());

                if (fieldJson.contains("value"))
                {
                    const auto& val = fieldJson["value"];
                    switch (field.m_Type)
                    {
                        case EScriptFieldType::FLOAT:
                            if (val.is_number())
                            {
                                field.m_FloatValue = val.get<float>();
                            }
                            break;
                        case EScriptFieldType::INT:
                            if (val.is_number())
                            {
                                field.m_IntValue = val.get<int>();
                            }
                            break;
                        case EScriptFieldType::BOOL:
                            if (val.is_boolean())
                            {
                                field.m_BoolValue = val.get<bool>();
                            }
                            break;
                        case EScriptFieldType::STRING:
                            if (val.is_string())
                            {
                                field.m_StringValue = val.get<std::string>();
                            }
                            break;
                        case EScriptFieldType::VEC3:
                            if (val.is_array() && val.size() >= 3)
                            {
                                field.m_Vec3Value = { val[0].get<float>(), val[1].get<float>(), val[2].get<float>() };
                            }
                            break;
                        case EScriptFieldType::COLOR:
                            if (val.is_array() && val.size() >= 4)
                            {
                                field.m_ColorValue = {
                                    static_cast<unsigned char>(val[0].get<int>()),
                                    static_cast<unsigned char>(val[1].get<int>()),
                                    static_cast<unsigned char>(val[2].get<int>()),
                                    static_cast<unsigned char>(val[3].get<int>())
                                };
                            }
                            break;
                        case EScriptFieldType::ENTITY_REF:
                            if (val.is_number_integer())
                            {
                                field.m_EntityIdValue = val.get<int>();
                            }
                            break;
                        case EScriptFieldType::ASSET_REF:
                            if (val.is_string())
                            {
                                field.m_AssetPathValue = val.get<std::string>();
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
            if (field.m_Name == name)
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
            if (field.m_Name == name)
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
            pField->m_FloatValue = value;
        }
    }

    void CScriptComponent::SetFieldValue(const std::string& name, int value)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_IntValue = value;
        }
    }

    void CScriptComponent::SetFieldValue(const std::string& name, bool value)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_BoolValue = value;
        }
    }

    void CScriptComponent::SetFieldValue(const std::string& name, const std::string& value)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_StringValue = value;
        }
    }

    void CScriptComponent::SetFieldValue(const std::string& name, const Vec3& value)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_Vec3Value = value;
        }
    }

    void CScriptComponent::SetFieldValue(const std::string& name, const Color& value)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_ColorValue = value;
        }
    }

    void CScriptComponent::SetFieldEntityId(const std::string& name, int entityId)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_EntityIdValue = entityId;
        }
    }

    void CScriptComponent::SetFieldAssetPath(const std::string& name, const std::string& assetPath)
    {
        SScriptField* pField = FindField(name);
        if (pField)
        {
            pField->m_AssetPathValue = assetPath;
        }
    }

    void CScriptComponent::SyncWithHeaderFields(const std::vector<SScriptField>& vHeaderFields)
    {
        std::vector<SScriptField> vUpdated;
        vUpdated.reserve(vHeaderFields.size());

        for (const auto& headerField : vHeaderFields)
        {
            const SScriptField* pExisting = FindField(headerField.m_Name);
            if (pExisting && pExisting->m_Type == headerField.m_Type)
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
            switch (field.m_Type)
            {
                case EScriptFieldType::FLOAT:
                {
                    currentOffset = (currentOffset + 3) & ~size_t(3);
                    auto* pDst = reinterpret_cast<float*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    *pDst = field.m_FloatValue;
                    m_pInstance->SetPropertyFloat(field.m_Name, field.m_FloatValue);
                    currentOffset += sizeof(float);
                    break;
                }
                case EScriptFieldType::INT:
                {
                    currentOffset = (currentOffset + 3) & ~size_t(3);
                    auto* pDst = reinterpret_cast<int*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    *pDst = field.m_IntValue;
                    m_pInstance->SetPropertyInt(field.m_Name, field.m_IntValue);
                    currentOffset += sizeof(int);
                    break;
                }
                case EScriptFieldType::BOOL:
                {
                    auto* pDst = reinterpret_cast<bool*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    *pDst = field.m_BoolValue;
                    m_pInstance->SetPropertyBool(field.m_Name, field.m_BoolValue);
                    currentOffset += sizeof(bool);
                    break;
                }
                case EScriptFieldType::VEC3:
                {
                    currentOffset = (currentOffset + 3) & ~size_t(3);
                    auto* pDst = reinterpret_cast<Vec3*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    *pDst = field.m_Vec3Value;
                    m_pInstance->SetPropertyVec3(field.m_Name, field.m_Vec3Value);
                    currentOffset += sizeof(Vec3);
                    break;
                }
                case EScriptFieldType::STRING:
                {
                    currentOffset = (currentOffset + alignof(std::string) - 1) & ~(alignof(std::string) - 1);
                    auto* pDst = reinterpret_cast<std::string*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    *pDst = field.m_StringValue;
                    m_pInstance->SetPropertyString(field.m_Name, field.m_StringValue);
                    currentOffset += sizeof(std::string);
                    break;
                }
                case EScriptFieldType::COLOR:
                {
                    currentOffset = (currentOffset + alignof(Color) - 1) & ~(alignof(Color) - 1);
                    auto* pDst = reinterpret_cast<Color*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    *pDst = field.m_ColorValue;
                    m_pInstance->SetPropertyColor(field.m_Name, field.m_ColorValue);
                    currentOffset += sizeof(Color);
                    break;
                }
                case EScriptFieldType::ENTITY_REF:
                {
                    currentOffset = (currentOffset + alignof(SEntityRef) - 1) & ~(alignof(SEntityRef) - 1);
                    auto* pDst = reinterpret_cast<SEntityRef*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    pDst->m_EntityId = field.m_EntityIdValue;
                    pDst->m_pResolved = Scene::FindEntityById(field.m_EntityIdValue);
                    m_pInstance->SetPropertyEntityRef(field.m_Name, *pDst);
                    currentOffset += sizeof(SEntityRef);
                    break;
                }
                case EScriptFieldType::ASSET_REF:
                {
                    currentOffset = (currentOffset + alignof(SAssetRef) - 1) & ~(alignof(SAssetRef) - 1);
                    auto* pDst = reinterpret_cast<SAssetRef*>(reinterpret_cast<char*>(m_pInstance) + currentOffset);
                    pDst->m_Path = field.m_AssetPathValue;
                    m_pInstance->SetPropertyAssetRef(field.m_Name, *pDst);
                    currentOffset += sizeof(SAssetRef);
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
            switch (field.m_Type)
            {
                case EScriptFieldType::FLOAT:
                {
                    currentOffset = (currentOffset + 3) & ~size_t(3);
                    float val = 0.0f;
                    if (m_pInstance->GetPropertyFloat(field.m_Name, val))
                    {
                        field.m_FloatValue = val;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const float*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_FloatValue = *pSrc;
                    }
                    currentOffset += sizeof(float);
                    break;
                }
                case EScriptFieldType::INT:
                {
                    currentOffset = (currentOffset + 3) & ~size_t(3);
                    int val = 0;
                    if (m_pInstance->GetPropertyInt(field.m_Name, val))
                    {
                        field.m_IntValue = val;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const int*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_IntValue = *pSrc;
                    }
                    currentOffset += sizeof(int);
                    break;
                }
                case EScriptFieldType::BOOL:
                {
                    bool val = false;
                    if (m_pInstance->GetPropertyBool(field.m_Name, val))
                    {
                        field.m_BoolValue = val;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const bool*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_BoolValue = *pSrc;
                    }
                    currentOffset += sizeof(bool);
                    break;
                }
                case EScriptFieldType::VEC3:
                {
                    currentOffset = (currentOffset + 3) & ~size_t(3);
                    Vec3 val{};
                    if (m_pInstance->GetPropertyVec3(field.m_Name, val))
                    {
                        field.m_Vec3Value = val;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const Vec3*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_Vec3Value = *pSrc;
                    }
                    currentOffset += sizeof(Vec3);
                    break;
                }
                case EScriptFieldType::STRING:
                {
                    currentOffset = (currentOffset + alignof(std::string) - 1) & ~(alignof(std::string) - 1);
                    std::string val;
                    if (m_pInstance->GetPropertyString(field.m_Name, val))
                    {
                        field.m_StringValue = val;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const std::string*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_StringValue = *pSrc;
                    }
                    currentOffset += sizeof(std::string);
                    break;
                }
                case EScriptFieldType::COLOR:
                {
                    currentOffset = (currentOffset + alignof(Color) - 1) & ~(alignof(Color) - 1);
                    Color val{};
                    if (m_pInstance->GetPropertyColor(field.m_Name, val))
                    {
                        field.m_ColorValue = val;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const Color*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_ColorValue = *pSrc;
                    }
                    currentOffset += sizeof(Color);
                    break;
                }
                case EScriptFieldType::ENTITY_REF:
                {
                    currentOffset = (currentOffset + alignof(SEntityRef) - 1) & ~(alignof(SEntityRef) - 1);
                    SEntityRef val{};
                    if (m_pInstance->GetPropertyEntityRef(field.m_Name, val))
                    {
                        field.m_EntityIdValue = val.m_EntityId;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const SEntityRef*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_EntityIdValue = pSrc->m_EntityId;
                    }
                    currentOffset += sizeof(SEntityRef);
                    break;
                }
                case EScriptFieldType::ASSET_REF:
                {
                    currentOffset = (currentOffset + alignof(SAssetRef) - 1) & ~(alignof(SAssetRef) - 1);
                    SAssetRef val{};
                    if (m_pInstance->GetPropertyAssetRef(field.m_Name, val))
                    {
                        field.m_AssetPathValue = val.m_Path;
                    }
                    else
                    {
                        auto* pSrc = reinterpret_cast<const SAssetRef*>(reinterpret_cast<const char*>(m_pInstance) + currentOffset);
                        field.m_AssetPathValue = pSrc->m_Path;
                    }
                    currentOffset += sizeof(SAssetRef);
                    break;
                }
            }
        }
    }

} // quark::script
