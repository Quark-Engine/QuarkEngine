#include "engine/component_factory_registry.h"

#include <algorithm>

CComponentFactoryRegistry::CComponentFactoryRegistry()
{
    SeedBuiltIns();
}

CComponentFactoryRegistry::~CComponentFactoryRegistry() = default;

void CComponentFactoryRegistry::SeedBuiltIns()
{
    const auto seed = [this](const char* pTypeName, FComponentFactory create)
    {
        m_Factories[pTypeName] = SEntry{ std::move(create), true };
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

    const FComponentFactory textFactory = []()
    {
        return std::make_shared<CText3DComponent>();
    };
    seed("Text", textFactory);
    seed("3D Text", textFactory);
}

bool CComponentFactoryRegistry::Register(const std::string& typeName, FComponentFactory create)
{
    if (typeName.empty() || !create)
    {
        return false;
    }

    const auto existing = m_Factories.find(typeName);
    if (existing != m_Factories.end() && existing->second.builtIn)
    {
        return false;
    }

    m_Factories[typeName] = SEntry{ std::move(create), false };
    return true;
}

void CComponentFactoryRegistry::Unregister(const std::string& typeName)
{
    const auto existing = m_Factories.find(typeName);
    if (existing != m_Factories.end() && !existing->second.builtIn)
    {
        m_Factories.erase(existing);
    }
}

std::shared_ptr<IComponent> CComponentFactoryRegistry::Create(const std::string& typeName) const
{
    const auto found = m_Factories.find(typeName);
    if (found == m_Factories.end() || !found->second.create)
    {
        return nullptr;
    }
    return found->second.create();
}

bool CComponentFactoryRegistry::IsBuiltIn(const std::string& typeName) const
{
    const auto found = m_Factories.find(typeName);
    return found != m_Factories.end() && found->second.builtIn;
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
