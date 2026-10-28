#include "application_scene_renderer.h"
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
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
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
#elif defined(__linux__)
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
        ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    RendererType rendererType = RendererType::OpenGL;
    if (preferences.m_RendererBackend == 2)
    {
        rendererType = RendererType::Vulkan;
    }
#ifdef _WIN32
    else if (preferences.m_RendererBackend == 3)
    {
        rendererType = RendererType::D3D11;
    }
#endif
    InitWindow(1280, 720, dataPath.parent_path().filename().string().c_str(), rendererType);
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
    if (!preferences.m_VsyncEnabled && rendererType != RendererType::Vulkan)
    {
        SDL_GL_SetSwapInterval(0);
    }
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
    renderer.Initialize(GetCurrentBackend() == RendererType::Vulkan,
        preferences.m_ShadowMapSize, preferences.m_ShadowsEnabled,
        preferences.m_ShadowBias, preferences.m_ShadowFilterQuality, shaderPath);
    Camera3D camera{};
    camera.position = { 0.0f, 2.0f, 6.0f };
    camera.target = { 0.0f, 0.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose())
    {
        FindSceneCamera(scene, camera);
        renderer.PrepareFrameCache(scene);
        renderer.Update(scene, lights, camera.position, preferences.m_ShadowsEnabled,
            preferences.m_ShadowBias, preferences.m_ShadowFilterQuality);
        BeginDrawing();
        ClearBackground({
            static_cast<unsigned char>(preferences.m_BackgroundRed),
            static_cast<unsigned char>(preferences.m_BackgroundGreen),
            static_cast<unsigned char>(preferences.m_BackgroundBlue), 255
        });
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
            CEntityTextureService::DrawEntityWithTexture(
                entity, renderer.GetCachedWorldTransform(entityIndex), preferences);
        }
        EndMode3D();
        EndDrawing();
    }

    renderer.Unload();
    scene.ReleaseResources();
    assets.Unload();
    textMesh.Unload();
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
