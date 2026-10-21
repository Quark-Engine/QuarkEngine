#include "editor/runtime_builder.h"

#include "engine/scene_document.h"
#include "engine/scene_package.h"
#include "nlohmann/json.hpp"
#include "project.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{

std::string MakeSafeName(const std::string& name)
{
    std::string safeName = name;
    for (char& character : safeName)
    {
        if (character == '<' || character == '>' || character == ':' || character == '"' ||
            character == '/' || character == '\\' || character == '|' || character == '?' ||
            character == '*' || character == ';' || character == '$' || character == '\n' ||
            character == '\r')
        {
            character = '_';
        }
    }
    return safeName.empty() ? "Game" : safeName;
}

bool RunCommand(const std::string& command, std::string& errorMessage)
{
    const int result = std::system(command.c_str());
    if (result == 0)
    {
        return true;
    }
    errorMessage = "Runtime project build command failed";
    return false;
}

std::string QuotePath(const fs::path& path)
{
    return "\"" + path.generic_string() + "\"";
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

bool CopyRuntimeFile(const fs::path& executableDirectory,
                     const fs::path& outputDirectory,
                     const std::string& fileName,
                     std::string& errorMessage,
                     bool required = true)
{
    const fs::path sourcePath = executableDirectory / fileName;
    std::error_code error;
    if (!fs::is_regular_file(sourcePath, error))
    {
        if (!required)
        {
            return true;
        }
        errorMessage = "The engine release is missing runtime dependency: " + fileName;
        return false;
    }
    fs::copy_file(sourcePath, outputDirectory / fileName,
        fs::copy_options::overwrite_existing, error);
    if (error)
    {
        errorMessage = "Failed to copy runtime dependency " + fileName + ": " + error.message();
        return false;
    }
    return true;
}

bool PackageRuntimeShaders(const fs::path& dataPath,
                           const fs::path& executableDirectory,
                           std::string& errorMessage)
{
    const std::vector<std::string> vShaderNames = {
        "lighting.vs", "lighting.fs", "shadow_depth.vs", "shadow_depth.fs",
        "vulkan_lighting.vs", "vulkan_lighting.fs",
        "vulkan_shadow_depth.vs", "vulkan_shadow_depth.fs"
    };
    std::vector<std::pair<std::string, std::string>> vShaderEntries;
    const fs::path shaderDirectory = executableDirectory / "assets" / "shader";
    for (const std::string& shaderName : vShaderNames)
    {
        std::ifstream shaderFile(shaderDirectory / shaderName, std::ios::binary);
        if (!shaderFile)
        {
            errorMessage = "The engine release is missing shader: " + shaderName;
            return false;
        }
        const std::string shaderSource { std::istreambuf_iterator<char> { shaderFile },
            std::istreambuf_iterator<char> {} };
        vShaderEntries.emplace_back(shaderName, shaderSource);
    }
    return quark::CScenePackage::WriteFiles(dataPath / "shaders.pak",
        vShaderEntries, errorMessage);
}

bool BuildRuntimeProject(const fs::path& buildRoot,
                         const std::string& gameName, std::string& errorMessage)
{
    std::error_code error;
    const auto buildId = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path generatedProject = fs::temp_directory_path() /
        "QuarkEngineRuntime" / (gameName + "-" + std::to_string(buildId));
    fs::create_directories(generatedProject, error);
    if (error)
    {
        errorMessage = "Failed to create temporary runtime project: " + error.message();
        return false;
    }
    [[maybe_unused]] const auto generatedProjectCleanup = std::shared_ptr<void>(nullptr,
        [generatedProject](void*)
        {
            std::error_code cleanupError;
            fs::remove_all(generatedProject, cleanupError);
        });

    const fs::path entryPath = generatedProject / "main.cpp";
    std::ofstream entryFile(entryPath, std::ios::binary | std::ios::trunc);
    entryFile << R"(extern "C" __declspec(dllimport) int QuarkEngineRunGame();

int main()
{
    return QuarkEngineRunGame();
}
)";
    entryFile.close();
    if (!entryFile)
    {
        errorMessage = "Failed to generate the game entry point";
        return false;
    }

    const fs::path cmakePath = generatedProject / "CMakeLists.txt";
    std::ofstream cmakeFile(cmakePath, std::ios::binary | std::ios::trunc);
    cmakeFile << R"CMAKE(cmake_minimum_required(VERSION 3.16)
                    project(QuarkGame LANGUAGES CXX)

                    set(CMAKE_CXX_STANDARD 20)
                    set(CMAKE_CXX_STANDARD_REQUIRED ON)

                    add_library(QuarkEngine SHARED IMPORTED GLOBAL)
                    set_target_properties(QuarkEngine PROPERTIES
                        IMPORTED_IMPLIB "${ENGINE_SDK_DIR}/QuarkEngineRuntime.lib"
                        IMPORTED_LOCATION "${ENGINE_SDK_DIR}/QuarkEngine.dll"
                    )

                    add_executable(QuarkGame main.cpp)
                    target_link_libraries(QuarkGame PRIVATE QuarkEngine)
                    set_target_properties(QuarkGame PROPERTIES
                        OUTPUT_NAME "${GAME_EXECUTABLE_NAME}"
                        RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/$<0:>"
                    )
                    )CMAKE";
    cmakeFile.close();
    if (!cmakeFile)
    {
        errorMessage = "Failed to generate the game CMake project";
        return false;
    }

    const fs::path executableDirectory = GetExecutableDirectory();
    const fs::path cmakeBuild = generatedProject / "cmake-build";
    const std::string cmakeCommand = "cmake";
    const std::string configureCommand = cmakeCommand + " -S " + QuotePath(generatedProject) +
        " -B " + QuotePath(cmakeBuild) +
        " -DENGINE_SDK_DIR=" + QuotePath(executableDirectory) +
        " -DGAME_OUTPUT_DIR=" + QuotePath(buildRoot) +
        " -DGAME_EXECUTABLE_NAME=" + QuotePath(gameName);
    if (!RunCommand(configureCommand, errorMessage))
    {
        return false;
    }
    const std::string buildCommand = cmakeCommand + " --build " + QuotePath(cmakeBuild) +
#ifdef QUARK_ENGINE_CONFIGURATION
        " --config " + std::string(QUARK_ENGINE_CONFIGURATION) +
#else
        " --config Release" +
#endif
        " --target QuarkGame";
    if (!RunCommand(buildCommand, errorMessage))
    {
        return false;
    }

    const bool isDebugBuild =
#ifdef QUARK_ENGINE_CONFIGURATION
        std::string(QUARK_ENGINE_CONFIGURATION) == "Debug";
#else
        false;
#endif
    const std::string freetypeName = isDebugBuild ? "freetyped.dll" : "freetype.dll";
    const std::string assimpName = isDebugBuild ? "assimp-vc143-mtd.dll" : "assimp-vc143-mt.dll";
    const std::string curlName = isDebugBuild ? "libcurl-d.dll" : "libcurl.dll";
    for (const std::string& fileName : std::vector<std::string>{
        "QuarkCore.dll", "QuarkEngine.dll", "SDL3.dll", freetypeName, assimpName,
        "OpenAL32.dll", "vulkan-1.dll", "SPIRV-Tools.dll", "SPIRV-Tools-opt.dll"
    })
    {
        if (!CopyRuntimeFile(executableDirectory, buildRoot, fileName, errorMessage))
        {
            return false;
        }
    }
    for (const std::string& fileName : std::vector<std::string>{
        curlName, "libssl-3-x64.dll", "libcrypto-3-x64.dll", "nghttp2.dll"
    })
    {
        if (!CopyRuntimeFile(executableDirectory, buildRoot, fileName, errorMessage, false))
        {
            return false;
        }
    }

    return true;
}

bool LaunchGame(const fs::path& executablePath, const fs::path& dataPath,
                std::string& errorMessage)
{
#ifdef _WIN32
    const std::string command = "start \"\" " + QuotePath(executablePath) + " " + QuotePath(dataPath);
#else
    const std::string command = QuotePath(executablePath) + " " + QuotePath(dataPath) + " >/dev/null 2>&1 &";
#endif
    if (RunCommand(command, errorMessage))
    {
        return true;
    }
    errorMessage = "Failed to launch the built game";
    return false;
}

} // anonymous

bool CRuntimeBuilder::Build(const std::string& projectPath, const CScene& scene,
                            const CPreferences& preferences,
                            std::string& outputPath, std::string& errorMessage)
{
    const fs::path projectRoot = CProjectService::ResolveRoot(projectPath);
    if (projectRoot.empty())
    {
        errorMessage = "Could not locate the open project";
        return false;
    }
    const std::string gameName = MakeSafeName(projectRoot.filename().string());
    const fs::path buildRoot = projectRoot / "Build" / gameName;
    const fs::path dataPath = buildRoot / "data";
    std::error_code error;
    if (fs::exists(buildRoot, error))
    {
        fs::remove_all(buildRoot, error);
        if (error)
        {
            errorMessage = "Failed to clear the previous game build: " + error.message();
            return false;
        }
    }
    fs::create_directories(dataPath, error);
    if (error)
    {
        errorMessage = "Failed to create the game output directory: " + error.message();
        return false;
    }

    const fs::path projectResources = projectRoot / "resources";
    const bool hasProjectResources = fs::exists(projectResources, error) &&
                                     fs::is_directory(projectResources, error);
    if (error)
    {
        errorMessage = "Failed to inspect game resources: " + error.message();
        return false;
    }
    if (hasProjectResources)
    {
        for (const fs::directory_entry& entry : fs::directory_iterator(projectResources, error))
        {
            if (error)
            {
                break;
            }
            const std::string resourceName = entry.path().filename().string();
            if (resourceName == "scenes.pak" || resourceName == "settings.pak" ||
                resourceName == "shaders.pak")
            {
                errorMessage = "Project resources cannot use reserved package name: " + resourceName;
                return false;
            }
            fs::copy(entry.path(), dataPath / entry.path().filename(),
                fs::copy_options::recursive | fs::copy_options::overwrite_existing, error);
            if (error)
            {
                break;
            }
        }
    }
    if (error)
    {
        errorMessage = "Failed to copy game resources: " + error.message();
        return false;
    }

    const std::string sceneDocument = quark::CSceneDocument::Serialize(scene);
    if (!quark::CScenePackage::Write(dataPath / "scenes.pak", sceneDocument,
        projectRoot, errorMessage))
    {
        return false;
    }

    nlohmann::json runtimeConfig;
    runtimeConfig["editor_preferences"] = {
        { "renderer_backend", preferences.m_RendererBackend },
        { "msaa_samples", preferences.m_MsaaSamples },
        { "texture_filter", preferences.m_TextureFilter },
        { "vsync_enabled", preferences.m_VsyncEnabled },
        { "limit_fps", preferences.m_LimitFps },
        { "target_fps", preferences.m_TargetFps },
        { "background_red", preferences.m_BackgroundRed },
        { "background_green", preferences.m_BackgroundGreen },
        { "background_blue", preferences.m_BackgroundBlue },
        { "shadows_enabled", preferences.m_ShadowsEnabled },
        { "shadow_map_size", preferences.m_ShadowMapSize },
        { "shadow_bias", preferences.m_ShadowBias },
        { "shadow_filter_quality", preferences.m_ShadowFilterQuality }
    };
    const std::vector<std::pair<std::string, std::string>> vSettingsEntries = {
        { "settings.json", runtimeConfig.dump(4) }
    };
    if (!quark::CScenePackage::WriteFiles(dataPath / "settings.pak",
        vSettingsEntries, errorMessage))
    {
        return false;
    }
    if (!PackageRuntimeShaders(dataPath, GetExecutableDirectory(), errorMessage))
    {
        return false;
    }

    if (!BuildRuntimeProject(buildRoot, gameName, errorMessage))
    {
        return false;
    }
    outputPath = buildRoot.string();
    return true;
}

bool CRuntimeBuilder::BuildAndRun(const std::string& projectPath, const CScene& scene,
                                  const CPreferences& preferences,
                                  std::string& outputPath, std::string& errorMessage)
{
    if (!Build(projectPath, scene, preferences, outputPath, errorMessage))
    {
        return false;
    }
#ifdef _WIN32
    const fs::path executablePath = fs::path(outputPath) / (MakeSafeName(
        fs::path(projectPath).filename().string()) + ".exe");
#else
    const fs::path executablePath = fs::path(outputPath) /
        MakeSafeName(fs::path(projectPath).filename().string());
#endif
    return LaunchGame(executablePath, fs::path(outputPath) / "data", errorMessage);
}
