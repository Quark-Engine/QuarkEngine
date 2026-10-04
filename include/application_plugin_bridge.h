#ifndef __APPLICATION_PLUGIN_BRIDGE_H__
#define __APPLICATION_PLUGIN_BRIDGE_H__

#include "plugins/plugin_manager.h"

#include <memory>

class CEditor;
class CFlyCamera;

void DispatchPluginEvent(EPluginEvent event, int entityIndex = -1);

class CPluginBridge
{
public:
    void Initialize(CEditor& editor, CPluginManager& pluginManager);

    void SetCamera(CFlyCamera& camera);

    void Update(CEditor& editor, CPluginManager& pluginManager);

    void Reset();

    SPluginContext* GetContext() const { return m_pContext.get(); }

private:
    void SyncContext(CEditor& editor, CPluginManager& pluginManager);

    std::unique_ptr<SPluginContext> m_pContext;
    CFlyCamera* m_pCamera = nullptr;
};

#endif // __APPLICATION_PLUGIN_BRIDGE_H__
