#include "command_line.h"
#include "version.h"
#include <iostream>

namespace
{

bool HasValue(int argc, char** ppArgv, int index)
{
    return index + 1 < argc;
}

} // anonymous

void CCommandLineParser::PrintVersion()
{
    std::cout << "Quark Engine " << QUARK_ENGINE_VERSION << "\n";
}

void CCommandLineParser::PrintUsage(const char* pProgramName)
{
    std::cout <<
        "Usage: " << pProgramName << " [options] [project_path]\n"
        "\n"
        "General:\n"
        "  project_path              Path to the project to open (positional)\n"
        "  --project <path>          Same as above, explicit form\n"
        "  --headless                Run without a window / editor UI\n"
        "  --test                    Run a minimal startup check and exit (implies --headless)\n"
        "  --new-project             Force-recreate the project even if a valid one exists\n"
        "  -h, --help                Show this help text and exit\n"
        "  --version                 Show the engine version and exit\n"
        "\n"
        "Rendering:\n"
        "  --renderer <opengl|vulkan> Override the renderer backend from preferences\n"
        "  --vsync <on|off>           Override vsync from preferences\n"
        "  --fps <n>                  Override target FPS (0 = unlimited)\n"
        "\n"
        "Plugins:\n"
        "  --no-plugins               Don't load any plugins on startup\n"
        "  --plugins-dir <path>       Directory to load plugins from (default: \"plugins\")\n"
        "\n"
        "Misc:\n"
        "  --lang <code>               Override the editor language\n"
        "  --log-level <level>         Set log verbosity (trace|info|warn|error)\n"
        "  --no-autosave                Disable autosave for this session\n"
        "  --dump-frame [n]             Print full render state for the first n frames, then exit (debug)\n";
}

SCommandLineOptions CCommandLineParser::Parse(int argc, char** ppArgv)
{
    SCommandLineOptions options;

    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
    {
        const std::string argument = ppArgv[argumentIndex];

        if (argument == "--headless")
        {
            options.Headless = true;
        }
        else if (argument == "--test")
        {
            options.TestMode = true;
        }
        else if (argument == "--new-project")
        {
            options.NewProject = true;
        }
        else if (argument == "-h" || argument == "--help")
        {
            options.HelpRequested = true;
        }
        else if (argument == "--version")
        {
            options.VersionRequested = true;
        }
        else if (argument == "--no-plugins")
        {
            options.NoPlugins = true;
        }
        else if (argument == "--no-autosave")
        {
            options.NoAutosave = true;
        }
        else if (argument == "--project")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                options.ProjectPath = ppArgv[++argumentIndex];
            }
        }
        else if (argument == "--renderer")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                const std::string value = ppArgv[++argumentIndex];
                if (value == "opengl")
                {
                    options.RendererOverride = ERendererOverride::OPENGL;
                }
                else if (value == "vulkan")
                {
                    options.RendererOverride = ERendererOverride::VULKAN;
                }
                else
                {
                    std::cerr << "Unknown renderer '" << value << "', ignoring.\n";
                }
            }
        }
        else if (argument == "--vsync")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                const std::string value = ppArgv[++argumentIndex];
                if (value == "on")
                {
                    options.VsyncOverride = ETriState::ON;
                }
                else if (value == "off")
                {
                    options.VsyncOverride = ETriState::OFF;
                }
                else
                {
                    std::cerr << "Unknown vsync value '" << value << "', ignoring.\n";
                }
            }
        }
        else if (argument == "--fps")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                const std::string value = ppArgv[++argumentIndex];
                try
                {
                    options.FpsOverride = std::stoi(value);
                }
                catch (...)
                {
                    std::cerr << "Invalid --fps value '" << value << "', ignoring.\n";
                }
            }
        }
        else if (argument == "--plugins-dir")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                options.PluginsDir = ppArgv[++argumentIndex];
            }
        }
        else if (argument == "--lang")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                options.LangOverride = ppArgv[++argumentIndex];
            }
        }
        else if (argument == "--log-level")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                options.LogLevel = ppArgv[++argumentIndex];
            }
        }
        else if (argument == "--dump-frame")
        {
            if (HasValue(argc, ppArgv, argumentIndex))
            {
                try
                {
                    options.DumpFrames = std::stoi(ppArgv[argumentIndex + 1]);
                    if (options.DumpFrames > 0)
                    {
                        ++argumentIndex;
                    }
                }
                catch (...)
                {
                    options.DumpFrames = 2;
                }
            }
            else
            {
                options.DumpFrames = 2;
            }
        }
        else if (options.ProjectPath.empty())
        {
            options.ProjectPath = argument;
        }
    }

    options.Headless = options.Headless || options.TestMode;

    if (options.HelpRequested)
    {
        CCommandLineParser::PrintUsage(ppArgv[0]);
    }
    if (options.VersionRequested)
    {
        CCommandLineParser::PrintVersion();
    }

    return options;
}