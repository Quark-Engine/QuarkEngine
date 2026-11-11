#include "engine/component_factory_registry.h"
#include "scripting/script_component.h"

#include <algorithm>

CComponentFactoryRegistry::CComponentFactoryRegistry()
{
    SeedBuiltIns();
}

CComponentFactoryRegistry::~CComponentFactoryRegistry() = default;

void CComponentFactoryRegistry::SeedBuiltIns()
{
    const auto seed = [this](const char* pTypeName, FComponentFactory pfnCreate)
    {
        m_Factories[pTypeName] = SEntry{ std::move(pfnCreate), true };
    };

    seed("Transform", []()
         {
             return std::make_shared<CTransformComponent>();
         });
    seed("Mesh", []()
         {
             return std::make_shared<CMeshComponent>();
         });
    seed("Material", []()
         {
             return std::make_shared<CMaterialComponent>();
         });
    seed("Light", []()
         {
             return std::make_shared<CLightComponent>();
         });
    seed("Collision", []()
         {
             return std::make_shared<CCollisionComponent>();
         });
    seed("Camera", []()
         {
             return std::make_shared<CCameraComponent>();
         });

    seed("Text", []()
         {
             return std::make_shared<CTextComponent>();
         });
    const FComponentFactory pfnText3dFactory = []()
    {
        return std::make_shared<CText3DComponent>();
    };
    seed("3D Text", pfnText3dFactory);
    seed("Rect Transform", []()
         {
             return std::make_shared<CUiRectTransformComponent>();
         });
    seed("Script", []()
         {
             return std::make_shared<CScriptComponent>();
         });
    seed("UI", []()
         {
             return std::make_shared<CUiComponent>();
         });
    seed("Ui Button", []()
         {
             return std::make_shared<CUiComponent>();
         });
    seed("Ui Inputfield", []()
         {
             return std::make_shared<CUiComponent>();
         });
    seed("Ui Combo", []()
         {
             return std::make_shared<CUiComponent>();
         });
}

bool CComponentFactoryRegistry::Register(const std::string& typeName, FComponentFactory pfnCreate)
{
    if (typeName.empty() || !pfnCreate)
    {
        return false;
    }

    const auto existing = m_Factories.find(typeName);
    if (existing != m_Factories.end() && existing->second.m_BuiltIn)
    {
        return false;
    }

    m_Factories[typeName] = SEntry{ std::move(pfnCreate), false };
    return true;
}

void CComponentFactoryRegistry::Unregister(const std::string& typeName)
{
    const auto existing = m_Factories.find(typeName);
    if (existing != m_Factories.end() && !existing->second.m_BuiltIn)
    {
        m_Factories.erase(existing);
    }
}

std::shared_ptr<IComponent> CComponentFactoryRegistry::Create(const std::string& typeName) const
{
    const auto found = m_Factories.find(typeName);
    if (found == m_Factories.end() || !found->second.pfnCreate)
    {
        return nullptr;
    }
    return found->second.pfnCreate();
}

bool CComponentFactoryRegistry::IsBuiltIn(const std::string& typeName) const
{
    const auto found = m_Factories.find(typeName);
    return found != m_Factories.end() && found->second.m_BuiltIn;
}

std::vector<std::string> CComponentFactoryRegistry::TypeNames() const
{
    std::vector<std::string> vNames;
    vNames.reserve(m_Factories.size());
    for (const auto& [typeName, entry] : m_Factories)
    {
        vNames.push_back(typeName);
    }
    std::sort(vNames.begin(), vNames.end());
    return vNames;
}
