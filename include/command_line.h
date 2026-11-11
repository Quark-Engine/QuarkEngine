#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__
#include <string>

enum class ERendererOverride
{
    NONE,
    OPENGL,
    VULKAN,
    D3D11
};

enum class ETriState
{
    UNSET,
    ON,
    OFF
};

struct SCommandLineOptions
{
    bool m_Headless = false;
    bool m_TestMode = false;
    std::string m_ProjectPath;

    ERendererOverride m_RendererOverride = ERendererOverride::NONE;
    ETriState m_VsyncOverride = ETriState::UNSET;
    int m_FpsOverride = -1;

    bool m_NoPlugins = false;
    std::string m_PluginsDir = "plugins";

    std::string m_LangOverride;
    std::string m_LogLevel;

    bool m_NoAutosave = false;
    bool m_NewProject = false;

    bool m_HelpRequested = false;
    bool m_VersionRequested = false;

    int m_DumpFrames = 0;
};

class CCommandLineParser
{
public:
    static void PrintVersion();
    static void PrintUsage(const char* pProgramName);
    static SCommandLineOptions Parse(int argc, char** ppArgv);
};

#endif // __COMMAND_LINE_H__
