#include "scripting/script_engine.h"
#include "entity.h"
#include "project.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#endif

namespace fs = std::filesystem;

namespace quark::script
{

namespace
{

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty())
    {
        return L"";
    }
    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], sizeNeeded);
    return wstr;
}
#endif

std::string Trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string StripComments(const std::string& source)
{
    std::string result;
    result.reserve(source.size());

    bool inBlockComment = false;
    bool inLineComment = false;
    bool inString = false;

    for (size_t i = 0; i < source.size(); ++i)
    {
        char c = source[i];
        char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

        if (inBlockComment)
        {
            if (c == '*' && next == '/')
            {
                inBlockComment = false;
                ++i;
            }
            continue;
        }

        if (inLineComment)
        {
            if (c == '\n' || c == '\r')
            {
                inLineComment = false;
                result += c;
            }
            continue;
        }

        if (inString)
        {
            result += c;
            if (c == '\\' && next != '\0')
            {
                result += next;
                ++i;
            }
            else if (c == '"')
            {
                inString = false;
            }
            continue;
        }

        if (c == '/' && next == '*')
        {
            inBlockComment = true;
            ++i;
            continue;
        }

        if (c == '/' && next == '/')
        {
            inLineComment = true;
            ++i;
            continue;
        }

        if (c == '"')
        {
            inString = true;
            result += c;
            continue;
        }

        result += c;
    }

    return result;
}

fs::path GetExecutableDirectory()
{
#ifdef _WIN32
    std::wstring executablePath(MAX_PATH, L'\0');
    DWORD pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
        static_cast<DWORD>(executablePath.size()));
    while (pathLength == executablePath.size())
    {
        executablePath.resize(executablePath.size() * 2);
        pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
    }
    executablePath.resize(pathLength);
    return fs::path(executablePath).parent_path();
#else
    return fs::canonical("/proc/self/exe").parent_path();
#endif
}

std::string QuotePath(const fs::path& path)
{
    return "\"" + path.generic_string() + "\"";
}

bool RunBuildCommand(const std::string& command, std::string& errorMessage,
                     const CScriptEngine::FProgressCallback& progressCallback)
{
    if (progressCallback)
    {
        progressCallback("> " + command);
    }
#ifdef _WIN32
    std::wstring wideCommand = Utf8ToWide(command + " 2>&1");
    FILE* pPipe = _wpopen(wideCommand.c_str(), L"rt");
#else
    FILE* pPipe = popen((command + " 2>&1").c_str(), "r");
#endif
    if (pPipe == nullptr)
    {
        errorMessage = "Failed to start command: " + command;
        return false;
    }

    char aLine[1024];
    std::string pendingLine;
    std::string fullOutput;
    while (fgets(aLine, sizeof(aLine), pPipe) != nullptr)
    {
        fullOutput += aLine;
        pendingLine += aLine;
        size_t newlinePosition = pendingLine.find('\n');
        while (newlinePosition != std::string::npos)
        {
            if (progressCallback)
            {
                progressCallback(pendingLine.substr(0, newlinePosition));
            }
            pendingLine.erase(0, newlinePosition + 1);
            newlinePosition = pendingLine.find('\n');
        }
    }
    if (!pendingLine.empty() && progressCallback)
    {
        progressCallback(pendingLine);
    }
#ifdef _WIN32
    const int result = _pclose(pPipe);
#else
    const int result = pclose(pPipe);
#endif
    if (result == 0)
    {
        return true;
    }
    errorMessage = fullOutput.empty()
        ? ("Build command failed with exit code " + std::to_string(result))
        : fullOutput;
    return false;
}

} // anonymous

bool CScriptHeaderParser::ParseHeader(const std::string& filePath, SParsedScriptInfo& outInfo)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = StripComments(buffer.str());

    size_t classPos = content.find("class ");
    std::string className;
    size_t classBodyStart = std::string::npos;

    while (classPos != std::string::npos)
    {
        size_t nameStart = content.find_first_not_of(" \t\r\n", classPos + 6);
        if (nameStart == std::string::npos)
        {
            break;
        }

        size_t nameEnd = content.find_first_of(" \t\r\n:{", nameStart);
        if (nameEnd == std::string::npos)
        {
            break;
        }

        std::string candidateName = content.substr(nameStart, nameEnd - nameStart);

        size_t colonPos = content.find(':', nameEnd);
        size_t openBrace = content.find('{', nameEnd);

        if (colonPos != std::string::npos && colonPos < openBrace)
        {
            std::string inheritance = content.substr(colonPos + 1, openBrace - (colonPos + 1));
            if (inheritance.find("IScript") != std::string::npos)
            {
                className = candidateName;
                classBodyStart = openBrace + 1;
                break;
            }
        }

        classPos = content.find("class ", classPos + 6);
    }

    if (className.empty() || classBodyStart == std::string::npos)
    {
        return false;
    }

    outInfo.className = className;
    outInfo.headerPath = filePath;
    outInfo.vFields.clear();

    int braceDepth = 1;
    size_t classBodyEnd = std::string::npos;
    for (size_t i = classBodyStart; i < content.size(); ++i)
    {
        if (content[i] == '{')
        {
            ++braceDepth;
        }
        else if (content[i] == '}')
        {
            --braceDepth;
            if (braceDepth == 0)
            {
                classBodyEnd = i;
                break;
            }
        }
    }

    if (classBodyEnd == std::string::npos)
    {
        return false;
    }

    std::string body = content.substr(classBodyStart, classBodyEnd - classBodyStart);

    bool isPublic = false;
    size_t pos = 0;
    while (pos < body.size())
    {
        size_t nextSemicolon = body.find(';', pos);
        size_t nextColon = body.find(':', pos);

        if (nextColon != std::string::npos && (nextSemicolon == std::string::npos || nextColon < nextSemicolon))
        {
            std::string label = Trim(body.substr(pos, nextColon - pos));
            if (label == "public" || label.find("public") != std::string::npos)
            {
                isPublic = true;
            }
            else if (label == "private" || label == "protected" ||
                     label.find("private") != std::string::npos || label.find("protected") != std::string::npos)
            {
                isPublic = false;
            }
            pos = nextColon + 1;
            continue;
        }

        if (nextSemicolon == std::string::npos)
        {
            break;
        }

        std::string statement = Trim(body.substr(pos, nextSemicolon - pos));
        pos = nextSemicolon + 1;

        if (!isPublic || statement.empty())
        {
            continue;
        }

        if (statement.find('(') != std::string::npos || statement.find(')') != std::string::npos)
        {
            continue;
        }
        if (statement.rfind("using ", 0) == 0 || statement.rfind("typedef ", 0) == 0 ||
            statement.rfind("struct ", 0) == 0 || statement.rfind("class ", 0) == 0 ||
            statement.rfind("enum ", 0) == 0 || statement.rfind("friend ", 0) == 0)
        {
            continue;
        }

        std::string decl = statement;
        std::string defaultValStr;
        size_t eqPos = decl.find('=');
        if (eqPos != std::string::npos)
        {
            defaultValStr = Trim(decl.substr(eqPos + 1));
            decl = Trim(decl.substr(0, eqPos));
        }

        std::istringstream iss(decl);
        std::string typeToken;
        std::string nameToken;
        iss >> typeToken;
        if (typeToken == "unsigned" || typeToken == "signed" || typeToken == "std::")
        {
            std::string subType;
            iss >> subType;
            typeToken += " " + subType;
        }
        iss >> nameToken;

        nameToken = Trim(nameToken);
        if (nameToken.empty())
        {
            continue;
        }

        SScriptField field;
        field.name = nameToken;

        if (typeToken == "float" || typeToken == "double")
        {
            field.type = EScriptFieldType::FLOAT;
            if (!defaultValStr.empty())
            {
                try
                {
                    std::string num = defaultValStr;
                    if (!num.empty() && (num.back() == 'f' || num.back() == 'F'))
                    {
                        num.pop_back();
                    }
                    field.floatValue = std::stof(num);
                }
                catch (...)
                {
                    field.floatValue = 0.0f;
                }
            }
        }
        else if (typeToken == "int" || typeToken == "int32_t" || typeToken == "long")
        {
            field.type = EScriptFieldType::INT;
            if (!defaultValStr.empty())
            {
                try
                {
                    field.intValue = std::stoi(defaultValStr);
                }
                catch (...)
                {
                    field.intValue = 0;
                }
            }
        }
        else if (typeToken == "bool")
        {
            field.type = EScriptFieldType::BOOL;
            if (!defaultValStr.empty())
            {
                field.boolValue = (defaultValStr == "true" || defaultValStr == "1");
            }
        }
        else if (typeToken == "std::string" || typeToken == "string")
        {
            field.type = EScriptFieldType::STRING;
            if (!defaultValStr.empty())
            {
                if (defaultValStr.size() >= 2 && defaultValStr.front() == '"' && defaultValStr.back() == '"')
                {
                    field.stringValue = defaultValStr.substr(1, defaultValStr.size() - 2);
                }
                else
                {
                    field.stringValue = defaultValStr;
                }
            }
        }
        else if (typeToken == "Vec3" || typeToken == "Vector3" || typeToken == "qc::Vec3")
        {
            field.type = EScriptFieldType::VEC3;
            if (!defaultValStr.empty())
            {
                size_t braceStart = defaultValStr.find('{');
                size_t braceEnd = defaultValStr.find('}');
                if (braceStart != std::string::npos && braceEnd != std::string::npos)
                {
                    std::string coords = defaultValStr.substr(braceStart + 1, braceEnd - braceStart - 1);
                    std::replace(coords.begin(), coords.end(), ',', ' ');
                    std::istringstream ciss(coords);
                    ciss >> field.vec3Value.x >> field.vec3Value.y >> field.vec3Value.z;
                }
            }
        }
        else
        {
            continue;
        }

        outInfo.vFields.push_back(field);
    }

    return true;
}

std::vector<SParsedScriptInfo> CScriptHeaderParser::ScanProjectScripts(const std::string& projectPath)
{
    std::vector<SParsedScriptInfo> vResults;
    const fs::path root(projectPath);
    std::error_code ec;

    if (!fs::exists(root, ec))
    {
        return vResults;
    }

    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            break;
        }
        if (!entry.is_regular_file())
        {
            continue;
        }

        const fs::path path = entry.path();
        const std::string filename = path.filename().string();
        if (filename.rfind(".", 0) == 0)
        {
            continue;
        }

        const std::string ext = path.extension().string();
        if (ext == ".hpp" || ext == ".h")
        {
            SParsedScriptInfo info;
            if (ParseHeader(path.string(), info))
            {
                vResults.push_back(info);
            }
        }
    }

    return vResults;
}

SInputCallbacks g_InputCallbacks;

CScriptEngine::CScriptEngine()
{
    g_InputCallbacks.pfnIsKeyDown = [](int key) -> bool
    {
        return ::IsKeyDown(static_cast<KeyboardKey>(key));
    };
    g_InputCallbacks.pfnIsKeyPressed = [](int key) -> bool
    {
        return ::IsKeyPressed(static_cast<KeyboardKey>(key));
    };
    g_InputCallbacks.pfnIsKeyReleased = [](int key) -> bool
    {
        return ::IsKeyReleased(static_cast<KeyboardKey>(key));
    };
    g_InputCallbacks.pfnIsKeyUp = [](int key) -> bool
    {
        return ::IsKeyUp(static_cast<KeyboardKey>(key));
    };
    g_InputCallbacks.pfnIsMouseButtonDown = [](int button) -> bool
    {
        return ::IsMouseButtonDown(static_cast<MouseButton>(button));
    };
    g_InputCallbacks.pfnIsMouseButtonPressed = [](int button) -> bool
    {
        return ::IsMouseButtonPressed(static_cast<MouseButton>(button));
    };
    g_InputCallbacks.pfnIsMouseButtonReleased = [](int button) -> bool
    {
        return ::IsMouseButtonReleased(static_cast<MouseButton>(button));
    };
    g_InputCallbacks.pfnIsMouseButtonUp = [](int button) -> bool
    {
        return ::IsMouseButtonUp(static_cast<MouseButton>(button));
    };
    g_InputCallbacks.pfnGetMousePosition = []() -> Vec2
    {
        return ::GetMousePosition();
    };

    m_pActiveDll = &m_Dll;
}

CScriptEngine& CScriptEngine::Get()
{
    static CScriptEngine s_Instance;
    return s_Instance;
}

CScriptEngine::~CScriptEngine()
{
    UnloadScriptDll();
}

bool CScriptEngine::BuildScriptDll(const std::string& projectPath, const std::string& gameName,
                                   std::string& errorMessage,
                                   const FProgressCallback& progressCallback)
{
    const fs::path projectRoot(projectPath);
    std::error_code ec;

    std::vector<fs::path> vScriptSources;
    for (const auto& entry : fs::recursive_directory_iterator(projectRoot, ec))
    {
        if (ec)
        {
            break;
        }
        if (!entry.is_regular_file())
        {
            continue;
        }

        const fs::path p = entry.path();
        const std::string ext = p.extension().string();
        if (ext == ".cpp")
        {
            const std::string pathStr = p.generic_string();
            if (pathStr.find("/.script_build") != std::string::npos ||
                pathStr.find("/cmake-build") != std::string::npos ||
                pathStr.find("/Build") != std::string::npos)
            {
                continue;
            }
            vScriptSources.push_back(p);
        }
    }

    if (vScriptSources.empty())
    {
        errorMessage = "No script source files (.cpp) found in project";
        return false;
    }

    const fs::path buildDir = projectRoot / ".script_build";
    fs::create_directories(buildDir, ec);

    const fs::path executableDir = GetExecutableDirectory();
    const fs::path engineSourceDir = fs::current_path();

    fs::path quarkCoreLib = executableDir / "QuarkCore.lib";
    if (!fs::is_regular_file(quarkCoreLib))
    {
        quarkCoreLib = executableDir / "QuarkCoreRuntime.lib";
    }

    fs::path quarkEngineLib = executableDir / "QuarkEngineRuntime.lib";
    if (!fs::is_regular_file(quarkEngineLib))
    {
        quarkEngineLib = executableDir / "QuarkEngine.lib";
    }

    const fs::path bridgeCpp = buildDir / "script_bridge.cpp";
    std::ofstream bridgeOut(bridgeCpp, std::ios::binary | std::ios::trunc);
    if (bridgeOut.is_open())
    {
        bridgeOut << "#include \"QuarkEngine/SDK.h\"\n\n";
        bridgeOut << "namespace quark::script\n{\n";
        bridgeOut << "    SInputCallbacks g_InputCallbacks;\n";
        bridgeOut << "}\n\n";
        bridgeOut << "extern \"C\" QUARK_SCRIPT_API void SetScriptInputCallbacks(const ::quark::script::SInputCallbacks* pCallbacks)\n";
        bridgeOut << "{\n";
        bridgeOut << "    if (pCallbacks)\n";
        bridgeOut << "    {\n";
        bridgeOut << "        ::quark::script::g_InputCallbacks = *pCallbacks;\n";
        bridgeOut << "    }\n";
        bridgeOut << "}\n";
        bridgeOut.close();
    }

    const fs::path cmakeFile = buildDir / "CMakeLists.txt";
    std::ofstream cmakeOut(cmakeFile, std::ios::binary | std::ios::trunc);
    if (!cmakeOut.is_open())
    {
        errorMessage = "Failed to create CMakeLists.txt for script DLL";
        return false;
    }

    const std::string targetName = "GameScripts";

    cmakeOut << "cmake_minimum_required(VERSION 3.16)\n";
    cmakeOut << "project(" << targetName << " LANGUAGES CXX)\n\n";
    cmakeOut << "set(CMAKE_CXX_STANDARD 20)\n";
    cmakeOut << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";

    cmakeOut << "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << ")\n";
    cmakeOut << "set(CMAKE_LIBRARY_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << ")\n";
    cmakeOut << "set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << ")\n";
    cmakeOut << "foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})\n";
    cmakeOut << "    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)\n";
    cmakeOut << "    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} " << QuotePath(projectRoot) << ")\n";
    cmakeOut << "    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} " << QuotePath(projectRoot) << ")\n";
    cmakeOut << "    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} " << QuotePath(projectRoot) << ")\n";
    cmakeOut << "endforeach()\n\n";

    cmakeOut << "add_library(" << targetName << " SHARED\n";
    cmakeOut << "    " << QuotePath(bridgeCpp) << "\n";
    for (const auto& src : vScriptSources)
    {
        cmakeOut << "    " << QuotePath(src) << "\n";
    }
    cmakeOut << ")\n\n";

    cmakeOut << "set_target_properties(" << targetName << " PROPERTIES\n";
    cmakeOut << "    PREFIX \"\"\n";
    cmakeOut << "    OUTPUT_NAME \"" << targetName << "\"\n";
    cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY_DEBUG " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY_RELEASE " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY_DEBUG " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY_RELEASE " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO " << QuotePath(projectRoot) << "\n";
    cmakeOut << ")\n\n";

    cmakeOut << "target_compile_definitions(" << targetName << " PRIVATE QUARK_SCRIPT_BUILD_DLL)\n\n";

    cmakeOut << "target_include_directories(" << targetName << " PRIVATE\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "include") << "\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "include") << "\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "SDL3" / "include") << "\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "Vulkan" / "include") << "\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "OpenAL" / "include") << "\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "imgui" / "include") << "\n";
    cmakeOut << "    " << QuotePath(engineSourceDir / "src") << "\n";
    cmakeOut << "    " << QuotePath(projectRoot) << "\n";
    cmakeOut << "    " << QuotePath(projectRoot / "resources") << "\n";
    cmakeOut << ")\n\n";

    cmakeOut << "add_library(QuarkCore SHARED IMPORTED GLOBAL)\n";
    cmakeOut << "set_target_properties(QuarkCore PROPERTIES\n";
    cmakeOut << "    IMPORTED_IMPLIB " << QuotePath(quarkCoreLib) << "\n";
    cmakeOut << "    IMPORTED_LOCATION " << QuotePath(executableDir / "QuarkCore.dll") << "\n";
    cmakeOut << ")\n\n";

    cmakeOut << "add_library(QuarkEngine SHARED IMPORTED GLOBAL)\n";
    cmakeOut << "set_target_properties(QuarkEngine PROPERTIES\n";
    cmakeOut << "    IMPORTED_IMPLIB " << QuotePath(quarkEngineLib) << "\n";
    cmakeOut << "    IMPORTED_LOCATION " << QuotePath(executableDir / "QuarkEngine.dll") << "\n";
    cmakeOut << ")\n\n";

    fs::path sdl3Lib = engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "SDL3" / "lib";
#if defined(_DEBUG) || defined(DEBUG)
    sdl3Lib /= "debug";
#else
    sdl3Lib /= "release";
#endif
    sdl3Lib /= "SDL3.lib";
    if (!fs::is_regular_file(sdl3Lib))
    {
        sdl3Lib = executableDir / "SDL3.lib";
    }

    if (fs::is_regular_file(sdl3Lib))
    {
        cmakeOut << "add_library(SDL3 SHARED IMPORTED GLOBAL)\n";
        cmakeOut << "set_target_properties(SDL3 PROPERTIES\n";
        cmakeOut << "    IMPORTED_IMPLIB " << QuotePath(sdl3Lib) << "\n";
        cmakeOut << "    IMPORTED_LOCATION " << QuotePath(executableDir / "SDL3.dll") << "\n";
        cmakeOut << ")\n\n";
        cmakeOut << "target_link_libraries(" << targetName << " PRIVATE QuarkEngine QuarkCore SDL3)\n";
    }
    else
    {
        cmakeOut << "target_link_libraries(" << targetName << " PRIVATE QuarkEngine QuarkCore)\n";
    }
    cmakeOut.close();

    const fs::path cmakeBuild = buildDir / "build";
    const std::string configureCmd = "cmake -S " + QuotePath(buildDir) +
        " -B " + QuotePath(cmakeBuild);

    if (progressCallback)
    {
        progressCallback("Configuring script project...");
    }
    if (!RunBuildCommand(configureCmd, errorMessage, progressCallback))
    {
        return false;
    }

#if defined(_DEBUG) || defined(DEBUG)
    const std::string buildConfig = "Debug";
#else
    const std::string buildConfig = "Release";
#endif

    const std::string buildCmd = "cmake --build " + QuotePath(cmakeBuild) +
        " --config " + buildConfig + " --target " + targetName;
    if (progressCallback)
    {
        progressCallback("Compiling " + targetName + ".dll (" + buildConfig + ")...");
    }
    if (!RunBuildCommand(buildCmd, errorMessage, progressCallback))
    {
        return false;
    }

    const std::vector<fs::path> vOutputs = {
        cmakeBuild / buildConfig / (targetName + ".dll"),
        cmakeBuild / (targetName + ".dll"),
        cmakeBuild / "Debug" / (targetName + ".dll"),
        cmakeBuild / "Release" / (targetName + ".dll")
    };
    std::error_code copyEc;
    for (const auto& outPath : vOutputs)
    {
        if (fs::is_regular_file(outPath, copyEc))
        {
            fs::copy_file(outPath, fs::path(projectPath) / "GameScripts.dll", fs::copy_options::overwrite_existing, copyEc);
            fs::copy_file(outPath, fs::path(projectPath) / (gameName + ".dll"), fs::copy_options::overwrite_existing, copyEc);
            break;
        }
    }

    return true;
}

bool CScriptEngine::LoadScriptDll(const std::string& dllPath)
{
    UnloadScriptDll();

    if (!m_Dll.Open(dllPath))
    {
        TraceLog(LogLevel::Error, "SCRIPT",
            TextFormat("Failed to load script DLL '%s': %s", dllPath.c_str(), m_Dll.GetError().c_str()));
        return false;
    }

    m_LoadedDllPath = dllPath;

    using FSetScriptInputCallbacks = void (*)(const SInputCallbacks*);
    auto pfnSetInput = reinterpret_cast<FSetScriptInputCallbacks>(m_Dll.GetSymbol("SetScriptInputCallbacks"));
    if (pfnSetInput)
    {
        pfnSetInput(&g_InputCallbacks);
        TraceLog(LogLevel::Info, "SCRIPT", "Configured script input callbacks bridge");
    }
    else
    {
        TraceLog(LogLevel::Warn, "SCRIPT", "Script DLL does not export SetScriptInputCallbacks");
    }

    TraceLog(LogLevel::Info, "SCRIPT", TextFormat("Loaded script DLL '%s'", dllPath.c_str()));
    return true;
}

void CScriptEngine::UnloadScriptDll()
{
    if (m_Dll.IsOpen())
    {
        m_Dll.Close();
        m_LoadedDllPath.clear();
    }
}

bool CScriptEngine::IsDllLoaded() const
{
    return m_Dll.IsOpen();
}

IScript* CScriptEngine::CreateScript(const std::string& scriptName)
{
    CDynamicLibrary* pDll = m_pActiveDll ? m_pActiveDll : &m_Dll;
    if (pDll->IsOpen())
    {
        std::string symbol = "Create_" + scriptName;
        auto pfnCreate = reinterpret_cast<FScriptCreator>(pDll->GetSymbol(symbol.c_str()));
        if (pfnCreate)
        {
            return pfnCreate();
        }
    }

    return CScriptRegistry::Get().Create(scriptName);
}

void CScriptEngine::DestroyScript(const std::string& scriptName, IScript* pScript)
{
    if (!pScript)
    {
        return;
    }

    CDynamicLibrary* pDll = m_pActiveDll ? m_pActiveDll : &m_Dll;
    if (pDll->IsOpen())
    {
        std::string symbol = "Destroy_" + scriptName;
        auto pfnDestroy = reinterpret_cast<FScriptDestroyer>(pDll->GetSymbol(symbol.c_str()));
        if (pfnDestroy)
        {
            pfnDestroy(pScript);
            return;
        }
    }

    CScriptRegistry::Get().Destroy(scriptName, pScript);
}

void CScriptEngine::StartPlay(CScene& scene, const std::string& projectPath,
                              CDynamicLibrary* pExternalDll)
{
    if (m_IsPlaying)
    {
        return;
    }

    RefreshAvailableScripts(projectPath);

    std::string gameName = fs::path(projectPath).filename().string();
    if (gameName.empty())
    {
        gameName = "Game";
    }

    if (pExternalDll && pExternalDll->IsOpen())
    {
        m_pActiveDll = pExternalDll;
        using FSetScriptInputCallbacks = void (*)(const SInputCallbacks*);
        auto pfnSetInput = reinterpret_cast<FSetScriptInputCallbacks>(m_pActiveDll->GetSymbol("SetScriptInputCallbacks"));
        if (pfnSetInput)
        {
            pfnSetInput(&g_InputCallbacks);
        }
    }
    else if (m_Dll.IsOpen())
    {
        m_pActiveDll = &m_Dll;
    }
    else
    {
        m_pActiveDll = &m_Dll;
        const fs::path primaryDllPath = fs::path(projectPath) / "GameScripts.dll";
        const fs::path namedDllPath = fs::path(projectPath) / (gameName + ".dll");
        std::error_code ec;

        const std::vector<fs::path> vSearchPaths = {
            primaryDllPath,
            namedDllPath,
            GetExecutableDirectory() / "GameScripts.dll",
            GetExecutableDirectory() / (gameName + ".dll"),
            fs::path(projectPath) / "Debug" / "GameScripts.dll",
            fs::path(projectPath) / "Release" / "GameScripts.dll",
            fs::path(projectPath) / "Debug" / (gameName + ".dll"),
            fs::path(projectPath) / "Release" / (gameName + ".dll"),
            fs::path(projectPath) / "Build" / gameName / (gameName + ".dll"),
            fs::path(projectPath) / "Build" / gameName / "GameScripts.dll",
            fs::current_path() / "GameScripts.dll",
            fs::current_path() / (gameName + ".dll")
        };

        const bool isStandaloneRuntime = fs::exists(fs::path(projectPath) / "__settings") ||
                                         (fs::path(projectPath).filename().string().rfind("QuarkRuntime-", 0) == 0);

        bool needsBuild = false;
        fs::path activeDllPath;
        for (const auto& path : vSearchPaths)
        {
            if (fs::is_regular_file(path, ec))
            {
                activeDllPath = path;
                break;
            }
        }

        if (activeDllPath.empty())
        {
            if (!isStandaloneRuntime)
            {
                needsBuild = true;
            }
            else
            {
                TraceLog(LogLevel::Warn, "SCRIPT", "Standalone runtime: no script DLL found in search paths");
            }
        }
        else if (!isStandaloneRuntime)
        {
            auto dllWriteTime = fs::last_write_time(activeDllPath, ec);
            if (!ec)
            {
                for (const auto& entry : fs::recursive_directory_iterator(fs::path(projectPath), fs::directory_options::skip_permission_denied, ec))
                {
                    if (ec)
                    {
                        break;
                    }
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }
                    const std::string ext = entry.path().extension().string();
                    if (ext == ".cpp" || ext == ".hpp" || ext == ".h")
                    {
                        const std::string pStr = entry.path().generic_string();
                        if (pStr.find("/.script_build") != std::string::npos ||
                            pStr.find("/cmake-build") != std::string::npos ||
                            pStr.find("/Build") != std::string::npos)
                        {
                            continue;
                        }
                        if (fs::last_write_time(entry.path(), ec) > dllWriteTime)
                        {
                            needsBuild = true;
                            break;
                        }
                    }
                }
            }
        }

        bool loaded = false;
        if (!needsBuild && !activeDllPath.empty())
        {
            if (LoadScriptDll(activeDllPath.string()))
            {
                if (m_Dll.GetSymbol("SetScriptInputCallbacks"))
                {
                    loaded = true;
                }
                else if (!isStandaloneRuntime)
                {
                    UnloadScriptDll();
                    needsBuild = true;
                }
                else
                {
                    loaded = true;
                }
            }
        }

        if (!loaded && needsBuild && !isStandaloneRuntime)
        {
            std::string buildError;
            std::error_code removeEc;
            fs::remove(primaryDllPath, removeEc);
            fs::remove(namedDllPath, removeEc);

            if (BuildScriptDll(projectPath, gameName, buildError))
            {
                std::vector<fs::path> vCandidates = {
                    fs::path(projectPath) / ".script_build" / "build" / "Debug" / "GameScripts.dll",
                    fs::path(projectPath) / ".script_build" / "build" / "Release" / "GameScripts.dll",
                    fs::path(projectPath) / ".script_build" / "build" / "GameScripts.dll",
                    fs::path(projectPath) / "Debug" / "GameScripts.dll",
                    fs::path(projectPath) / "Release" / "GameScripts.dll",
                    primaryDllPath,
                    namedDllPath
                };
                for (const auto& cand : vCandidates)
                {
                    if (fs::is_regular_file(cand, ec))
                    {
                        if (cand != primaryDllPath)
                        {
                            std::error_code copyEc;
                            fs::copy_file(cand, primaryDllPath, fs::copy_options::overwrite_existing, copyEc);
                            fs::copy_file(cand, namedDllPath, fs::copy_options::overwrite_existing, copyEc);
                        }
                        const fs::path loadTarget = fs::is_regular_file(primaryDllPath, ec) ? primaryDllPath : cand;
                        if (LoadScriptDll(loadTarget.string()))
                        {
                            if (!m_Dll.GetSymbol("SetScriptInputCallbacks"))
                            {
                                UnloadScriptDll();
                                continue;
                            }
                            loaded = true;
                            break;
                        }
                    }
                }
            }
            else
            {
                TraceLog(LogLevel::Error, "SCRIPT",
                    TextFormat("Failed to build script DLL for '%s':\n%s",
                        gameName.c_str(), buildError.c_str()));
            }
        }
    }

    for (auto& entity : scene.m_vEntities)
    {
        CScriptComponent* pScriptComp = entity.GetScriptComponent();
        if (!pScriptComp || pScriptComp->m_ScriptName.empty())
        {
            continue;
        }

        IScript* pInstance = CreateScript(pScriptComp->m_ScriptName);
        if (!pInstance)
        {
            TraceLog(LogLevel::Warn, "SCRIPT",
                TextFormat("Could not instantiate script '%s' on entity '%s'",
                    pScriptComp->m_ScriptName.c_str(), entity.m_Name.c_str()));
            continue;
        }

        pScriptComp->m_pInstance = pInstance;
        CTransformComponent* pTransform = entity.GetTransformComponent();
        if (pTransform)
        {
            pTransform->ClearLocalMatrixOverride();
        }
        Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
        Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
        Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
        bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
        pScriptComp->m_pScriptEntity = std::make_unique<CScriptEntity>(&entity, pPos, pRot, pScale, pHasOverride);
        pInstance->m_Entity = pScriptComp->m_pScriptEntity.get();
        pScriptComp->ApplyFieldsToInstance();

        pInstance->OnCreate();
    }

    for (auto& entity : scene.m_vEntities)
    {
        CScriptComponent* pScriptComp = entity.GetScriptComponent();
        if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
        {
            pScriptComp->m_pInstance->OnStart();
        }
    }

    m_IsPlaying = true;
}

void CScriptEngine::UpdatePlay(CScene& scene, float deltaTime)
{
    if (!m_IsPlaying)
    {
        return;
    }

    for (auto& entity : scene.m_vEntities)
    {
        CScriptComponent* pScriptComp = entity.GetScriptComponent();
        if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
        {
            pScriptComp->m_pInstance->OnUpdate(deltaTime);
        }
    }
}

void CScriptEngine::StopPlay(CScene& scene)
{
    if (!m_IsPlaying)
    {
        return;
    }

    for (auto& entity : scene.m_vEntities)
    {
        CScriptComponent* pScriptComp = entity.GetScriptComponent();
        if (pScriptComp && pScriptComp->m_pInstance)
        {
            pScriptComp->m_pInstance->OnDestroy();
            DestroyScript(pScriptComp->m_ScriptName, pScriptComp->m_pInstance);
            pScriptComp->m_pInstance = nullptr;
            pScriptComp->m_pScriptEntity.reset();
        }
    }

    UnloadScriptDll();
    m_pActiveDll = &m_Dll;
    m_IsPlaying = false;
}

void CScriptEngine::RefreshAvailableScripts(const std::string& projectPath)
{
    m_vAvailableScripts = CScriptHeaderParser::ScanProjectScripts(projectPath);
}

std::string CScriptEngine::FindScriptHeaderPath(const std::string& scriptName) const
{
    for (const auto& info : m_vAvailableScripts)
    {
        if (info.className == scriptName)
        {
            return info.headerPath;
        }
    }
    return "";
}

} // quark::script
