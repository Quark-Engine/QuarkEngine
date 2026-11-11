#include "application_scene_renderer.h"
#include "gui/gui_renderer_bridge.h"
#include "scripting/script_engine.h"
#include "assets/asset_library.h"
#include "editor/editor_preferences.h"
#include "engine/scene_package.h"
#include "engine/cpu_task_pool.h"
#include "engine/transform.h"
#include "project.h"
#include "scene.h"
#include "text_mesh.h"
#include "tex.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <SDL3/SDL_video.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#define NOMINMAX
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
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{

    bool FindSceneCamera(const CScene& scene, Camera3D& camera)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            const CEntity& entity = scene.m_vEntities[entityIndex];
            const CCameraComponent* pCamera = entity.GetCameraComponent();
            const CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pCamera || !pTransform || !pCamera->m_Enabled)
            {
                continue;
            }
            const Mat4 worldTransform = quark::ComposeWorld(scene, entityIndex);
            const Vec3 position = Vec3(worldTransform * Vec3{ 0.0f, 0.0f, 0.0f });
            Vec3 forward = Vec3(worldTransform * Vec3{ 0.0f, 0.0f, -1.0f }) - position;
            Vec3 up = Vec3(worldTransform * Vec3{ 0.0f, 1.0f, 0.0f }) - position;
            if (forward.length() <= 0.000001f || up.length() <= 0.000001f)
            {
                continue;
            }
            forward = forward.normalized();
            up = up.normalized();
            camera.position = position;
            camera.target = position + forward;
            camera.up = up;
            camera.fovy = pCamera->m_FieldOfView;
            return true;
        }
        return false;
    }

} // anonymous

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#else
    __attribute__((visibility("default")))
#endif
    int QuarkEngineRunGame()
    {
        fs::path executablePath;
#ifdef _WIN32
        std::wstring executableBuffer(32768, L'\0');
        const DWORD executableLength = GetModuleFileNameW(nullptr, executableBuffer.data(),
                                                          static_cast<DWORD>(executableBuffer.size()));
        if (executableLength == 0 || executableLength >= executableBuffer.size())
        {
            std::cerr << "Could not locate the game executable\n";
            return 1;
        }
        executableBuffer.resize(executableLength);
        executablePath = fs::path(executableBuffer);
#elif defined(__APPLE__)
        uint32_t executableSize = 0;
        _NSGetExecutablePath(nullptr, &executableSize);
        std::string executableBuffer(executableSize, '\0');
        if (_NSGetExecutablePath(&executableBuffer[0], &executableSize) == 0)
        {
            executablePath = fs::canonical(executableBuffer.c_str());
        }
        else
        {
            executablePath = fs::current_path() / "QuarkRuntime";
        }
#elif defined(__linux__) || defined(__unix__)
        std::string executableBuffer(4096, '\0');
        const ssize_t executableLength = readlink("/proc/self/exe", executableBuffer.data(),
                                                  executableBuffer.size() - 1);
        if (executableLength <= 0)
        {
            std::cerr << "Could not locate the game executable\n";
            return 1;
        }
        executableBuffer.resize(static_cast<size_t>(executableLength));
        executablePath = fs::path(executableBuffer);
#else
        executablePath = fs::current_path() / "QuarkRuntime";
#endif
        executablePath = fs::absolute(executablePath);
        fs::current_path(executablePath.parent_path());
        const fs::path dataPath = executablePath.parent_path() / "data";
        const auto runId = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path projectPath = fs::temp_directory_path() /
            ("QuarkRuntime-" + executablePath.stem().string() + "-" + std::to_string(runId));
        if (!quark::CScenePackage::Extract(dataPath / "scenes.pak", projectPath))
        {
            std::cerr << "Could not load data/scenes.pak\n";
            std::error_code cleanupError;
            fs::remove_all(projectPath, cleanupError);
            return 1;
        }
        std::error_code error;
        const fs::path resourcePath = projectPath / "resources";
        fs::create_directories(resourcePath, error);
        if (error)
        {
            std::cerr << "Could not prepare project resources\n";
            fs::remove_all(projectPath, error);
            return 1;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(dataPath, error))
        {
            if (error)
            {
                break;
            }
            const std::string entryName = entry.path().filename().string();
            if (entryName == "scenes.pak" || entryName == "settings.pak" ||
                entryName == "shaders.pak")
            {
                continue;
            }
            fs::copy(entry.path(), resourcePath / entry.path().filename(),
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing, error);
            if (error)
            {
                break;
            }
        }
        if (error)
        {
            std::cerr << "Could not load packaged project resources\n";
            fs::remove_all(projectPath, error);
            return 1;
        }

        const fs::path settingsPath = projectPath / "__settings";
        const fs::path shaderPath = projectPath / "__shaders";
        if (!quark::CScenePackage::Extract(dataPath / "settings.pak", settingsPath) ||
            !quark::CScenePackage::Extract(dataPath / "shaders.pak", shaderPath))
        {
            std::cerr << "Could not load packaged runtime settings or shaders\n";
            fs::remove_all(projectPath, error);
            return 1;
        }

        nlohmann::json runtimeSettings;
        std::ifstream settingsFile(settingsPath / "settings.json");
        try
        {
            settingsFile >> runtimeSettings;
        }
        catch (...)
        {
            std::cerr << "Could not read packaged runtime settings\n";
            fs::remove_all(projectPath, error);
            return 1;
        }
        std::string startScene = "scene.json";
        if (runtimeSettings.is_object() && runtimeSettings.contains("start_scene") &&
            runtimeSettings["start_scene"].is_string())
        {
            startScene = runtimeSettings["start_scene"].get<std::string>();
        }
        const fs::path startSceneRelativePath(startScene);
        if (startSceneRelativePath.empty() || startSceneRelativePath.is_absolute() ||
            startSceneRelativePath.has_root_name())
        {
            std::cerr << "Invalid packaged start scene path\n";
            fs::remove_all(projectPath, error);
            return 1;
        }
        for (const fs::path& component : startSceneRelativePath)
        {
            if (component == ".." || component == ".")
            {
                std::cerr << "Invalid packaged start scene path\n";
                fs::remove_all(projectPath, error);
                return 1;
            }
        }
        const fs::path startScenePath = projectPath / startSceneRelativePath;
        if (!fs::is_regular_file(startScenePath, error) || error)
        {
            std::cerr << "Packaged start scene is missing\n";
            fs::remove_all(projectPath, error);
            return 1;
        }

        CPreferences preferences;
        preferences.LoadFromPath(settingsPath / "settings.json");
        SetMSAASamples(preferences.m_MsaaSamples);
        SetTextureFilterMode(preferences.m_TextureFilter == 0
                                 ? TEXTURE_FILTER_POINT
                                 : TEXTURE_FILTER_BILINEAR);
        RendererType rendererType = RendererType::Auto;
        if (preferences.m_RendererBackend == 1)
        {
            rendererType = RendererType::OpenGL;
        }
        else if (preferences.m_RendererBackend == 2)
        {
            rendererType = RendererType::Vulkan;
        }
#ifdef _WIN32
        if (preferences.m_RendererBackend == 3)
        {
            rendererType = RendererType::D3D11;
        }
#endif
        InitWindow(1280, 720, dataPath.parent_path().filename().string().c_str(), rendererType);
        SetExitKey(KEY_NULL);
#ifdef _WIN32
        HICON pGameIcon = nullptr;
        const fs::path gameIconPath = settingsPath / "game.ico";
        if (fs::is_regular_file(gameIconPath))
        {
            pGameIcon = static_cast<HICON>(LoadImageW(nullptr, gameIconPath.c_str(), IMAGE_ICON,
                                                      0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
            if (pGameIcon != nullptr)
            {
                const SDL_PropertiesID windowProperties =
                    SDL_GetWindowProperties(GetNativeWindow());
                HWND pWindowHandle = static_cast<HWND>(SDL_GetPointerProperty(
                    windowProperties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
                if (pWindowHandle != nullptr)
                {
                    SendMessageW(pWindowHandle, WM_SETICON, ICON_SMALL,
                                 reinterpret_cast<LPARAM>(pGameIcon));
                    SendMessageW(pWindowHandle, WM_SETICON, ICON_BIG,
                                 reinterpret_cast<LPARAM>(pGameIcon));
                }
            }
        }
#endif
        SetVSync(preferences.m_VsyncEnabled);
        SetTargetFPS(preferences.m_LimitFps ? preferences.m_TargetFps : 0);

        CFreetypeTextMesh textMesh;
        textMesh.Init();
        CTaskPool taskPool;
        CAssetLibrary assets;
        assets.SetTextMesh(textMesh);
        assets.SetTaskPool(taskPool);
        assets.Load(projectPath.string());

        CScene scene;
        CLightRegistry lights;
        CComponentFactoryRegistry factories;
        if (!CProjectService::Load(startScenePath.string(), scene, assets, lights, factories))
        {
            std::cerr << "Could not load the packaged scene\n";
            assets.Unload();
            textMesh.Unload();
#ifdef _WIN32
            if (pGameIcon != nullptr)
            {
                DestroyIcon(pGameIcon);
            }
#endif
            fs::remove_all(projectPath, error);
            CloseWindow();
            return 1;
        }

        CSceneRenderer renderer;
        renderer.SetTaskPool(taskPool);
        renderer.Initialize(preferences.m_ShadowMapSize, preferences.m_ShadowsEnabled,
                            preferences.m_ShadowBias, preferences.m_ShadowFilterQuality, shaderPath);
        Camera3D camera{};
        camera.position = { 0.0f, 2.0f, 6.0f };
        camera.target = { 0.0f, 0.0f, 0.0f };
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        const std::string gameName = executablePath.stem().string();
        const std::string libExt =
#if defined(_WIN32)
            ".dll";
#elif defined(__APPLE__)
            ".dylib";
#else
            ".so";
#endif

        const std::vector<std::string> vScriptNames = {
            gameName + libExt,
            "lib" + gameName + libExt,
            "GameScripts" + libExt,
            "libGameScripts" + libExt,
            "GameName" + libExt
        };

        std::vector<fs::path> vDllCandidates;
        for (const auto& name : vScriptNames)
        {
            vDllCandidates.push_back(executablePath.parent_path() / name);
        }
#if !defined(_WIN32)
        vDllCandidates.push_back(executablePath.parent_path() / (gameName + ".so"));
        vDllCandidates.push_back(executablePath.parent_path() / ("lib" + gameName + ".so"));
        vDllCandidates.push_back(executablePath.parent_path() / "GameScripts.so");
        vDllCandidates.push_back(executablePath.parent_path() / "libGameScripts.so");
        vDllCandidates.push_back(executablePath.parent_path() / (gameName + ".dylib"));
        vDllCandidates.push_back(executablePath.parent_path() / ("lib" + gameName + ".dylib"));
        vDllCandidates.push_back(executablePath.parent_path() / "GameScripts.dylib");
        vDllCandidates.push_back(executablePath.parent_path() / "libGameScripts.dylib");
#endif

        fs::path foundDllPath;
        for (const auto& cand : vDllCandidates)
        {
            if (fs::is_regular_file(cand))
            {
                foundDllPath = cand;
                break;
            }
        }

        if (!foundDllPath.empty())
        {
            std::error_code copyEc;
            fs::copy_file(foundDllPath, projectPath / ("GameScripts" + libExt), fs::copy_options::overwrite_existing, copyEc);
            fs::copy_file(foundDllPath, projectPath / (gameName + libExt), fs::copy_options::overwrite_existing, copyEc);

            quark::script::CScriptEngine::Get().LoadScriptDll(foundDllPath.string());
        }

        quark::script::CScriptEngine::Get().StartPlay(scene, projectPath.string());
        quark::script::CScriptEngine::Get().SetCursorLocked(!quark::gui::SceneHasActiveUiWidgets(scene));

        while (!WindowShouldClose())
        {
            const bool hasSceneUi = quark::gui::SceneHasActiveUiWidgets(scene);
            if (IsKeyPressed(KEY_ESCAPE) && quark::script::CScriptEngine::Get().IsCursorLocked())
            {
                quark::script::CScriptEngine::Get().SetCursorLocked(false);
            }
            else if (!hasSceneUi &&
                     !quark::gui::CGuiSystem::Get().WantsMouseCapture() &&
                     !quark::script::CScriptEngine::Get().IsCursorLocked() &&
                     IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                quark::script::CScriptEngine::Get().SetCursorLocked(true);
            }

            const float deltaTime = static_cast<float>(GetFrameTime());
            quark::script::CScriptEngine::Get().UpdatePlay(scene, deltaTime);

            FindSceneCamera(scene, camera);
            renderer.PrepareFrameCache(scene);
            BeginDrawing();
            renderer.Update(scene, lights, camera.position, preferences.m_ShadowsEnabled,
                            preferences.m_ShadowBias, preferences.m_ShadowFilterQuality);
            ClearBackground({ static_cast<unsigned char>(preferences.m_BackgroundRed),
                              static_cast<unsigned char>(preferences.m_BackgroundGreen),
                              static_cast<unsigned char>(preferences.m_BackgroundBlue), 255 });
            BeginMode3D(camera);
            for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
            {
                CEntity& entity = scene.m_vEntities[entityIndex];
                CMeshComponent* pMesh = entity.GetMeshComponent();
                if (!pMesh || !pMesh->m_Enabled)
                {
                    continue;
                }
                renderer.EnsureLightingShader(pMesh);
                CMaterialComponent* pMaterial = entity.GetMaterialComponent();
                renderer.SetUseTexture(pMaterial && pMaterial->m_Texture.id != 0);
                renderer.BeginLightingPass();
                CEntityTextureService::DrawEntityWithTexture(
                    entity, renderer.GetCachedWorldTransform(entityIndex), preferences);
                renderer.EndLightingPass();
            }
            EndMode3D();
            quark::gui::RenderSceneGui(scene, GetMousePosition(), true);
            EndDrawing();
        }

        quark::script::CScriptEngine::Get().StopPlay(scene);

        renderer.Unload();
        scene.ReleaseResources();
        assets.Unload();
        textMesh.Unload();
        quark::gui::ShutdownGuiBackend();
        CloseWindow();
#ifdef _WIN32
        if (pGameIcon != nullptr)
        {
            DestroyIcon(pGameIcon);
        }
#endif
        fs::remove_all(projectPath, error);
        return 0;
    }
} // extern "C"
