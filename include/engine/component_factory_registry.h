#ifndef __ENGINE_COMPONENT_FACTORY_REGISTRY_H__
#define __ENGINE_COMPONENT_FACTORY_REGISTRY_H__
#include "../component.h"

#include <string>
#include <unordered_map>
#include <vector>

class CComponentFactoryRegistry
{
public:
    CComponentFactoryRegistry();
    ~CComponentFactoryRegistry();

    CComponentFactoryRegistry(const CComponentFactoryRegistry&) = delete;
    CComponentFactoryRegistry& operator=(const CComponentFactoryRegistry&) = delete;

    bool Register(const std::string& typeName, FComponentFactory create);

    void Unregister(const std::string& typeName);

    std::shared_ptr<IComponent> Create(const std::string& typeName) const;

    bool IsBuiltIn(const std::string& typeName) const;

    std::vector<std::string> TypeNames() const;

private:
    struct SEntry
    {
        FComponentFactory create;
        bool builtIn = false;
    };

    void SeedBuiltIns();

    std::unordered_map<std::string, SEntry> m_Factories;
};

#endif // __ENGINE_COMPONENT_FACTORY_REGISTRY_H__
