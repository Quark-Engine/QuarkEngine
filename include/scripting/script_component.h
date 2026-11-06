#ifndef __SCRIPT_COMPONENT_H__
#define __SCRIPT_COMPONENT_H__

#include "component.h"
#include "QuarkEngine/SDK.h"
#include "nlohmann/json.hpp"
#include <memory>
#include <string>
#include <vector>

namespace quark::script
{

enum class EScriptFieldType
{
    FLOAT,
    INT,
    BOOL,
    STRING,
    VEC3
};

struct SScriptField
{
    std::string name;
    EScriptFieldType type = EScriptFieldType::FLOAT;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    std::string stringValue;
    Vec3 vec3Value = {0.0f, 0.0f, 0.0f};
};

class CScriptComponent : public IComponent
{
public:
    std::string m_ScriptName;
    std::string m_ScriptPath;
    std::vector<SScriptField> m_vFields;

    IScript* m_pInstance = nullptr;
    std::unique_ptr<CScriptEntity> m_pScriptEntity;

    CScriptComponent();
    ~CScriptComponent() override;

    CScriptComponent(const CScriptComponent& other);
    CScriptComponent& operator=(const CScriptComponent& other);

    EComponentType GetType() const override;
    std::string GetTypeName() const override;

    void Serialize(nlohmann::json& json) const override;
    void Deserialize(const nlohmann::json& json) override;

    SScriptField* FindField(const std::string& name);
    const SScriptField* FindField(const std::string& name) const;

    void SetFieldValue(const std::string& name, float value);
    void SetFieldValue(const std::string& name, int value);
    void SetFieldValue(const std::string& name, bool value);
    void SetFieldValue(const std::string& name, const std::string& value);
    void SetFieldValue(const std::string& name, const Vec3& value);

    void SyncWithHeaderFields(const std::vector<SScriptField>& vHeaderFields);
    void ApplyFieldsToInstance();
    void ReadFieldsFromInstance();
};

} // quark::script

using CScriptComponent = ::quark::script::CScriptComponent;

#endif // __SCRIPT_COMPONENT_H__
