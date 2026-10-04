#ifndef __APPLICATION_H__
#define __APPLICATION_H__
#include "QuarkCore/QuarkCore.hpp"
#include "application_plugin_bridge.h"
#include "application_scene_renderer.h"
#include "camera.h"
#include "command_line.h"
#include "editor/editor.h"
#include "hub.h"
#include "plugins/plugin_manager.h"

#include <string>

class CApplication
{
public:
    explicit CApplication(const SCommandLineOptions& options);
    ~CApplication();

    CApplication(const CApplication&) = delete;
    CApplication& operator=(const CApplication&) = delete;

    void Initialize();

    void Run();

    void Shutdown();

    void Unload();

private:
    void UpdateFrame();
    void RenderFrame();

    SCommandLineOptions m_Options;

    CEditor m_Editor;
    CFlyCamera m_Camera;
    CHubApp m_Hub;

    CPluginManager m_PluginManager;
    CPluginBridge m_PluginBridge;
    CSceneRenderer m_SceneRenderer;

    std::string m_ProjectPath;
    std::string m_ActiveFontLanguage;
    double m_LastAutosaveTime = 0.0;
    int m_LastSelectedEntity = -1;

    bool m_Headless = false;
    bool m_ReadyToRun = false;
    bool m_WindowOpen = false;
};

#endif // __APPLICATION_H__
