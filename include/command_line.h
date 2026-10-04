#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__
#include <string>

enum class ERendererOverride
{
    NONE,
    OPENGL,
    VULKAN
};

enum class ETriState
{
    UNSET,
    ON,
    OFF
};

struct SCommandLineOptions
{
    bool Headless = false;
    bool TestMode = false;
    std::string ProjectPath;

    ERendererOverride RendererOverride = ERendererOverride::NONE;
    ETriState VsyncOverride = ETriState::UNSET;
    int FpsOverride = -1;

    bool NoPlugins = false;
    std::string PluginsDir = "plugins";

    std::string LangOverride;
    std::string LogLevel;

    bool NoAutosave = false;
    bool NewProject = false;

    bool HelpRequested = false;
    bool VersionRequested = false;

    int DumpFrames = 0;
};

class CCommandLineParser
{
public:
    static void PrintVersion();
    static void PrintUsage(const char* pProgramName);
    static SCommandLineOptions Parse(int argc, char** ppArgv);
};

#endif // __COMMAND_LINE_H__