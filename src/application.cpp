#include "application.h"
#include "gui/gui_renderer_bridge.h"
#include "scripting/script_engine.h"
#include "qcImGui.h"
#include "imgui.h"
#include "plugins/plugin_manager.h"
#include "language_manager.h"
#include "editor/editor_preferences.h"
#include "text_mesh.h"
#include "editor/editor_theme.h"
#include "editor/editor_entity.h"
#include "editor/editor_grid.h"
#include "editor/editor_viewers.h"
#include "project.h"
#include "hub.h"
#include "tex.h"
#include "engine/transform.h"
#include "engine/scene_runtime.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <SDL3/SDL_video.h>
namespace fs = std::filesystem;

namespace
{
    static std::ofstream s_EngineLogFile;
    static std::mutex s_EngineLogMutex;

    void WriteEngineLog(LogLevel, const char* pMessage)
    {
        if (pMessage == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(s_EngineLogMutex);
        if (s_EngineLogFile.is_open())
        {
            s_EngineLogFile << pMessage << '\n';
            s_EngineLogFile.flush();
        }
    }

    void InitializeEngineLog()
    {
        s_EngineLogFile.open("engine_log.txt", std::ios::out | std::ios::trunc);
        if (!s_EngineLogFile.is_open())
        {
            std::cerr << "Failed to open engine_log.txt for writing\n";
        }
        SetTraceLogCallback(WriteEngineLog);
    }

    float ElapsedMilliseconds(const std::chrono::steady_clock::time_point& start)
    {
        return std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - start)
            .count();
    }

    bool FindSceneCamera(const CScene& scene, Camera3D& camera)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            const CEntity& entity = scene.m_vEntities[entityIndex];
            const CCameraComponent* pCamera = entity.GetCameraComponent();
            if (!pCamera || !pCamera->m_Enabled)
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

CApplication::CApplication(const SCommandLineOptions& options)
    : m_Options(options)
{
}

CApplication::~CApplication()
{
}

static void ApplyLogLevel(const std::string& level)
{
    if (level.empty())
    {
        return;
    }
    std::cout << "[log-level] requested '" << level << "\n";
}

void CApplication::Initialize()
{
    InitializeEngineLog();

    if (m_Options.m_HelpRequested || m_Options.m_VersionRequested)
    {
        return;
    }

    fs::create_directories("projects");
    fs::create_directories("assets");

    if (m_Options.m_TestMode)
    {
        return;
    }

    ApplyLogLevel(m_Options.m_LogLevel);

    std::string langCode = LoadOrCreateConfig();
    if (!m_Options.m_LangOverride.empty())
    {
        langCode = m_Options.m_LangOverride;
    }
    m_Editor.m_Preferences.Load();
    CLanguageManager::Get().SetLang(langCode);

    RendererType rendererType = RendererType::Auto;
    if (m_Editor.m_Preferences.m_RendererBackend == 1)
    {
        rendererType = RendererType::OpenGL;
    }
    else if (m_Editor.m_Preferences.m_RendererBackend == 2)
    {
        rendererType = RendererType::Vulkan;
    }
    else if (m_Editor.m_Preferences.m_RendererBackend == 3)
    {
        rendererType = RendererType::D3D11;
    }
    if (m_Options.m_RendererOverride == ERendererOverride::OPENGL)
    {
        rendererType = RendererType::OpenGL;
    }
    else if (m_Options.m_RendererOverride == ERendererOverride::VULKAN)
    {
        rendererType = RendererType::Vulkan;
    }
    else if (m_Options.m_RendererOverride == ERendererOverride::D3D11)
    {
        rendererType = RendererType::D3D11;
    }

    SetMSAASamples(m_Editor.m_Preferences.m_MsaaSamples);
    SetTextureFilterMode(m_Editor.m_Preferences.m_TextureFilter == 0
                             ? TEXTURE_FILTER_POINT
                             : TEXTURE_FILTER_BILINEAR);

    InitWindow(1280, 720, "Quark Engine", rendererType);
    MaximizeWindow();
    m_WindowOpen = true;
    m_Camera.InitializeCursors();

    if (m_Options.m_FpsOverride < 0 && m_Editor.m_Preferences.m_TargetFps <= 0)
    {
        m_Editor.m_Preferences.m_TargetFps = static_cast<int>(GetCurrentMonitorRefreshRate());
    }

    const bool vsyncEnabled = m_Options.m_VsyncOverride == ETriState::ON ? true
        : m_Options.m_VsyncOverride == ETriState::OFF                    ? false
                                                                         : m_Editor.m_Preferences.m_VsyncEnabled;
    SetVSync(vsyncEnabled);

    if (m_Options.m_FpsOverride >= 0)
    {
        SetTargetFPS(m_Options.m_FpsOverride);
    }
    else
    {
        SetTargetFPS(m_Editor.m_Preferences.m_LimitFps ? m_Editor.m_Preferences.m_TargetFps : 0);
    }

    if (!vsyncEnabled)
    {
        SDL_GL_SetSwapInterval(0);
    }

    SetExitKey(KEY_NULL);

    m_Headless = m_Options.m_Headless;

    if (!m_Headless)
    {
        m_Editor.m_Text.Init();
        QcImGuiSetup(false);
        if (!CThemeManager::Apply(m_Editor.m_Preferences.m_ThemeName))
        {
            m_Editor.m_Preferences.m_ThemeName = "quark-dark";
            m_Editor.m_Preferences.m_LightTheme = false;
            CThemeManager::Apply(m_Editor.m_Preferences.m_ThemeName);
        }
        ImGui::GetStyle().ScaleAllSizes(m_Editor.m_Preferences.m_InterfaceScale);
        ImGui::GetStyle().FontScaleMain = m_Editor.m_Preferences.m_InterfaceScale *
            CThemeManager::GetAppliedFontScale();
        if (ImGui::GetStyle().WindowBorderHoverPadding <= 0.0f)
        {
            ImGui::GetStyle().WindowBorderHoverPadding = 1.0f;
        }
        if (ImGui::GetStyle().SeparatorSize <= 0.0f)
        {
            ImGui::GetStyle().SeparatorSize = 1.0f;
        }
    }

    m_ProjectPath = m_Options.m_ProjectPath;

    if (m_ProjectPath.empty() && !m_Headless)
    {
        if (m_Editor.m_Preferences.m_OpenLastProject &&
            !m_Editor.m_Preferences.m_LastProjectPath.empty() &&
            CProjectService::IsValid(m_Editor.m_Preferences.m_LastProjectPath))
        {
            m_ProjectPath = m_Editor.m_Preferences.m_LastProjectPath;
        }
        else
        {
            m_ProjectPath = m_Hub.Run(m_Editor.m_Preferences);
        }
        if (m_ProjectPath.empty())
        {
            Unload();
            return;
        }
    }

    if (m_ProjectPath.empty())
    {
        m_ProjectPath = (fs::path("projects") / "default").string();
    }

    m_ProjectPath = CProjectService::ResolveRoot(m_ProjectPath);
    m_Editor.m_Preferences.m_LastProjectPath = m_ProjectPath;
    m_Editor.m_Preferences.Save();

    fs::create_directories(fs::path(m_ProjectPath) / "resources");

    m_Editor.m_ProjectPath = m_ProjectPath;
    m_Camera.m_Speed = m_Editor.m_Preferences.m_CameraSpeed;
    m_Camera.m_ZoomSensitivity = m_Editor.m_Preferences.m_CameraZoomSensitivity;
    m_Camera.m_Cam.fovy = m_Editor.m_Preferences.m_CameraFov;

    m_SceneRenderer.SetTaskPool(m_Editor.m_CpuTaskPool);
    m_SceneRenderer.Initialize(
        m_Editor.m_Preferences.m_ShadowMapSize,
        m_Editor.m_Preferences.m_ShadowsEnabled,
        m_Editor.m_Preferences.m_ShadowBias,
        m_Editor.m_Preferences.m_ShadowFilterQuality);

    m_PluginBridge.Initialize(m_Editor, m_PluginManager);
    m_PluginBridge.SetCamera(m_Camera);

    m_Editor.m_Assets.Load(m_ProjectPath);
    m_Editor.m_Assets.Refresh(m_ProjectPath, m_Editor.m_Scene);

    if (CProjectService::IsValid(m_ProjectPath) && !m_Options.m_NewProject)
    {
        CProjectService::Load(m_ProjectPath, m_Editor.m_Scene, m_Editor.m_Assets, m_Editor.m_Lights, m_Editor.m_ComponentFactories);
    }
    else
    {
        CProjectService::CreateNew(m_ProjectPath, m_Editor.m_Scene);
        quark::CSceneRuntime::RestoreSceneEntityModels(m_Editor.m_Scene, m_Editor.m_Assets);
        quark::CSceneRuntime::ResetSceneLightRuntime(m_Editor.m_Scene, m_Editor.m_Lights);
    }

    if (m_Headless)
    {
        Unload();
        return;
    }

    if (!m_Options.m_NoPlugins)
    {
        m_Editor.m_pPluginManager = &m_PluginManager;
        m_PluginManager.SetDisabledPlugins(m_Editor.m_Preferences.m_vDisabledPlugins);
        m_PluginManager.LoadAll(m_Options.m_PluginsDir.c_str(), m_PluginBridge.GetContext());
    }
    DispatchPluginEvent(PLUGIN_EVENT_SCENE_LOADED);
    m_Editor.m_pPluginManager = &m_PluginManager;

    m_ActiveFontLanguage = CLanguageManager::Get().m_Current;
    m_LastAutosaveTime = GetTime();
    m_LastSelectedEntity = m_Editor.m_Scene.m_Selected;

#ifdef _DEBUG
    EnableDebugInterface();
#endif

    m_ReadyToRun = true;
}

void CApplication::UpdateFrame()
{
    m_Editor.PollHistoryRestore();
    if (m_Editor.IsHistoryRestorePending())
    {
        return;
    }

    m_SceneRenderer.PrepareFrameCache(m_Editor.m_Scene);

    if (m_ActiveFontLanguage != CLanguageManager::Get().m_Current)
    {
        m_ActiveFontLanguage = CLanguageManager::Get().m_Current;
        CThemeManager::ReloadFonts(m_Editor.m_Preferences.m_ThemeName);
    }

    m_Camera.m_Speed = m_Editor.m_Preferences.m_CameraSpeed;
    m_Camera.m_Sensitivity = m_Editor.m_Preferences.m_CameraSensitivity;
    m_Camera.m_ZoomSensitivity = m_Editor.m_Preferences.m_CameraZoomSensitivity;
    const Vec2 mousePosition = GetMousePosition();
    const ImVec2 viewportPosition = m_Editor.m_Ui.m_Viewport.m_WindowPos;
    const ImVec2 viewportSize = m_Editor.m_Ui.m_Viewport.m_WindowSize;
    const bool buildSettingsOpen = m_Editor.m_BuildSettingsModalOpen;
    const bool anyPopupOpen = ImGui::GetCurrentContext() != nullptr &&
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool imguiBlockingViewport = ImGui::GetCurrentContext() != nullptr &&
        (ImGui::IsAnyItemHovered() || anyPopupOpen);
    const bool cursorOverSceneViewport =
        m_Editor.m_Ui.m_Viewport.m_Hovered &&
        !imguiBlockingViewport &&
        mousePosition.x >= viewportPosition.x &&
        mousePosition.x <= viewportPosition.x + viewportSize.x &&
        mousePosition.y >= viewportPosition.y &&
        mousePosition.y <= viewportPosition.y + viewportSize.y;
    if (m_Editor.m_Preferences.m_FocusOnSelection && m_Editor.m_Scene.m_Selected != m_LastSelectedEntity)
    {
        CEntity* pSelectedEntity = m_Editor.m_Scene.GetSelected();
        CTransformComponent* pSelectedTransform = pSelectedEntity ? pSelectedEntity->GetTransformComponent() : nullptr;
        if (pSelectedTransform)
        {
            const int selectedIndex = m_Editor.m_Scene.m_Selected;
            const Vec3 worldPosition = Vec3Transform(
                Vec3{ 0.0f, 0.0f, 0.0f },
                m_SceneRenderer.GetCachedWorldTransform(selectedIndex));
            m_Camera.FocusOn(worldPosition);
        }
    }
    m_LastSelectedEntity = m_Editor.m_Scene.m_Selected;

    const bool activeViewportHovered = m_Editor.m_GameViewActive
        ? m_Editor.m_Ui.m_GameViewport.m_Hovered
        : m_Editor.m_Ui.m_Viewport.m_Hovered;
    quark::script::CScriptEngine::Get().SetViewportHovered(activeViewportHovered);

    if (m_Editor.m_IsPlaying)
    {
        const float deltaTime = static_cast<float>(GetFrameTime());
        quark::script::CScriptEngine::Get().UpdatePlay(m_Editor.m_Scene, deltaTime);
        m_SceneRenderer.PrepareFrameCache(m_Editor.m_Scene);
    }

    if (m_Editor.m_Scene2DMode)
    {
        const float viewportWidth = std::max(1.0f, m_Editor.m_Ui.m_Viewport.m_WindowSize.x);
        const float viewportHeight = std::max(1.0f, m_Editor.m_Ui.m_Viewport.m_WindowSize.y);
        if (m_Editor.m_Ui.m_Viewport.m_Hovered)
        {
            const float wheel = GetMouseWheelMove();
            if (std::fabs(wheel) > 0.001f)
            {
                m_Editor.m_Scene2DZoom = std::clamp(
                    m_Editor.m_Scene2DZoom * std::pow(1.1f, wheel), 0.1f, 8.0f);
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            {
                const Vec2 mouseDelta = GetMouseDelta();
                m_Editor.m_Scene2DPan.x -= mouseDelta.x / m_Editor.m_Scene2DZoom;
                m_Editor.m_Scene2DPan.y -= mouseDelta.y / m_Editor.m_Scene2DZoom;
            }
        }

        const float viewHeight = viewportHeight / m_Editor.m_Scene2DZoom;
        const Vec3 target = {
            viewportWidth * 0.5f + m_Editor.m_Scene2DPan.x,
            viewportHeight * 0.5f + m_Editor.m_Scene2DPan.y,
            0.0f
        };
        m_Camera.m_Cam.position = { target.x, target.y, -100.0f };
        m_Camera.m_Cam.target = target;
        m_Camera.m_Cam.up = { 0.0f, -1.0f, 0.0f };
        m_Camera.m_Cam.fovy = viewHeight;
        m_Camera.m_Cam.projection = CAMERA_ORTHOGRAPHIC;
    }

    if (!m_Editor.m_Scene2DMode &&
        !buildSettingsOpen && !m_Editor.m_IsPlaying && !m_Editor.m_GameViewActive &&
        (m_Camera.m_Active || (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && cursorOverSceneViewport)))
    {
        m_Camera.Update(m_Editor.m_Scene, m_Editor.m_Preferences);
    }

    if (!m_Options.m_NoAutosave && m_Editor.m_Preferences.m_AutosaveEnabled &&
        GetTime() - m_LastAutosaveTime >= m_Editor.m_Preferences.m_AutosaveIntervalMinutes * 60.0)
    {
        if (m_Editor.m_Preferences.m_AutosaveBackupEnabled)
        {
            std::error_code backupError;
            fs::copy_file(
                fs::path(m_Editor.m_ProjectPath) / "scene.json",
                fs::path(m_Editor.m_ProjectPath) / "scene.json.bak",
                fs::copy_options::overwrite_existing,
                backupError);
        }
        CProjectService::Save(m_Editor.m_ProjectPath, m_Editor.m_Scene);
        DispatchPluginEvent(PLUGIN_EVENT_SCENE_SAVED);
        m_LastAutosaveTime = GetTime();
    }

    const std::string projectTitle = fs::path(m_Editor.m_ProjectPath).filename().string();
    SetWindowTitle(TextFormat("Quark Engine | %s%s | FPS: %d",
                              projectTitle.c_str(), m_Editor.m_SceneDirty ? "*" : "", GetFPS()));

    Camera3D gameCamera = m_Camera.GetCamera();
    m_Editor.m_GameCameraAvailable = FindSceneCamera(m_Editor.m_Scene, gameCamera);
}

void CApplication::RenderFrame()
{
    m_Editor.m_CpuTaskPool.SetMainThreadTask("Render editor and submit GPU work");
    const auto renderStart = CTaskPool::CurrentThreadCpuTime();
    Camera3D gameCamera = m_Camera.GetCamera();
    m_Editor.m_GameCameraAvailable = FindSceneCamera(m_Editor.m_Scene, gameCamera);
    const Camera3D& renderCamera = m_Camera.GetCamera();
    BeginDrawing();
    const Vec3& renderCameraPosition = m_Editor.m_GameViewActive &&
            m_Editor.m_GameCameraAvailable
        ? gameCamera.position
        : renderCamera.position;
    m_SceneRenderer.Update(m_Editor.m_Scene, m_Editor.m_Lights, renderCameraPosition, m_Editor.m_Preferences.m_ShadowsEnabled,
                           m_Editor.m_Preferences.m_ShadowBias, m_Editor.m_Preferences.m_ShadowFilterQuality);
    ClearBackground(DARKGRAY);

    if (m_Editor.m_Ui.m_Viewport.m_RenderTexture.id > 0 && IsRenderTextureValid(m_Editor.m_Ui.m_Viewport.m_RenderTexture))
    {
        BeginTextureMode(m_Editor.m_Ui.m_Viewport.m_RenderTexture);
        ClearBackground(Color{ static_cast<unsigned char>(m_Editor.m_Preferences.m_BackgroundRed),
                               static_cast<unsigned char>(m_Editor.m_Preferences.m_BackgroundGreen),
                               static_cast<unsigned char>(m_Editor.m_Preferences.m_BackgroundBlue), 255 });
        m_FrameTimings.m_Main3dPassMs = 0.0f;
        if (!m_Editor.m_Scene2DMode)
        {
            const auto main3dStart = std::chrono::steady_clock::now();
            BeginMode3D(renderCamera);
            if (m_Editor.m_Preferences.m_ShowGrid)
            {
                CInfiniteGrid::Draw(m_Camera.GetCamera(), (int)m_Editor.m_Ui.m_Viewport.m_WindowSize.x, (int)m_Editor.m_Ui.m_Viewport.m_WindowSize.y, 1.0f,
                                    GRAY);
            }
            if (m_Editor.m_Preferences.m_ShowAxes)
            {
                DrawLine3D({ 0, 0, 0 }, { 3, 0, 0 }, RED);
                DrawLine3D({ 0, 0, 0 }, { 0, 3, 0 }, GREEN);
                DrawLine3D({ 0, 0, 0 }, { 0, 0, 3 }, BLUE);
            }
            const float viewportWidth = m_Editor.m_Ui.m_Viewport.m_WindowSize.x;
            const float viewportHeight = m_Editor.m_Ui.m_Viewport.m_WindowSize.y;
            const float aspectRatio = viewportHeight > 0.0f ? viewportWidth / viewportHeight : 1.0f;
            for (int entityIndex = 0; entityIndex < static_cast<int>(m_Editor.m_Scene.m_vEntities.size()); ++entityIndex)
            {
                auto& e = m_Editor.m_Scene.m_vEntities[entityIndex];
                CMeshComponent* pMesh = e.GetMeshComponent();
                CTransformComponent* pTransform = e.GetTransformComponent();
                CMaterialComponent* pMat = e.GetMaterialComponent();
                if (!pMesh || !pMesh->m_Enabled || !pTransform)
                {
                    continue;
                }

                const Mat4& worldTransform = m_SceneRenderer.GetCachedWorldTransform(entityIndex);
                if (!m_SceneRenderer.IsMeshVisible(entityIndex, renderCamera, aspectRatio))
                {
                    continue;
                }

                m_SceneRenderer.EnsureLightingShader(pMesh);
                bool hasTexture = (pMat && pMat->m_Texture.id != 0);
                if (!hasTexture && pMesh && pMesh->m_Model.materialCount > 0 && pMesh->m_Model.materials)
                {
                    for (int matIdx = 0; matIdx < pMesh->m_Model.materialCount; ++matIdx)
                    {
                        if (pMesh->m_Model.materials[matIdx].maps && (pMesh->m_Model.materials[matIdx].maps[MATERIAL_MAP_ALBEDO].texture.valid || pMesh->m_Model.materials[matIdx].maps[MATERIAL_MAP_ALBEDO].texture.id != 0))
                        {
                            hasTexture = true;
                            break;
                        }
                    }
                }
                m_SceneRenderer.SetUseTexture(hasTexture);
                m_SceneRenderer.BeginLightingPass();
                CEntityTextureService::DrawEntityWithTexture(e, worldTransform, m_Editor.m_Preferences);
                m_SceneRenderer.EndLightingPass();
                if (m_Editor.m_Preferences.m_ShowSelectionVisualization && m_Editor.m_Scene.IsSelected(entityIndex) && pMesh->m_Model.meshCount > 0)
                {
                    PushMatrix();
                    MultMatrix(m_SceneRenderer.GetCachedMeshWorldTransform(entityIndex));
                    const bool primarySelection = entityIndex == m_Editor.m_Scene.m_Selected;
                    DrawBoundingBox(GetModelBoundingBox(pMesh->m_Model), Color{ static_cast<unsigned char>(m_Editor.m_Preferences.m_SelectionRed), static_cast<unsigned char>(m_Editor.m_Preferences.m_SelectionGreen), static_cast<unsigned char>(m_Editor.m_Preferences.m_SelectionBlue), static_cast<unsigned char>(primarySelection ? 255 : 150) });
                    PopMatrix();
                }
                if (m_Editor.m_Preferences.m_ShowBoundingBoxes && pMesh->m_Model.meshCount > 0)
                {
                    PushMatrix();
                    MultMatrix(m_SceneRenderer.GetCachedMeshWorldTransform(entityIndex));
                    DrawBoundingBox(GetModelBoundingBox(pMesh->m_Model), Color{ static_cast<unsigned char>(m_Editor.m_Preferences.m_BoundsRed), static_cast<unsigned char>(m_Editor.m_Preferences.m_BoundsGreen), static_cast<unsigned char>(m_Editor.m_Preferences.m_BoundsBlue), 255 });
                    PopMatrix();
                }
            }
            if (m_Editor.m_Preferences.m_ShowLightHelpers)
            {
                for (int entityIndex = 0; entityIndex < static_cast<int>(m_Editor.m_Scene.m_vEntities.size()); ++entityIndex)
                {
                    auto& entity = m_Editor.m_Scene.m_vEntities[entityIndex];
                    CLightComponent* pLight = entity.GetLightComponent();
                    CTransformComponent* pTransform = entity.GetTransformComponent();
                    if (!pLight || !pTransform || !pLight->m_Enabled)
                    {
                        continue;
                    }
                    const Mat4& worldTransform = m_SceneRenderer.GetCachedWorldTransform(entityIndex);
                    const Vec3 worldPosition = Vec3(worldTransform * Vec3{ 0.0f, 0.0f, 0.0f });
                    DrawLine3D(worldPosition, pLight->m_Light.m_Target, pLight->m_Light.m_Color);
                }
            }
            if (m_Editor.m_Preferences.m_ShowCameras)
            {
                const Camera3D& editorCamera = m_Camera.GetCamera();
                const Vec3 forward = (editorCamera.target - editorCamera.position).normalized();
                const Vec3 right = forward.cross(editorCamera.up).normalized();
                const Vec3 up = right.cross(forward).normalized();
                const float length = 1.5f;
                const float halfWidth = tanf(editorCamera.fovy * DEG2RAD * 0.5f) * length;
                const Vec3 center = editorCamera.position + forward * length;
                const Vec3 aCorners[4] = { center + up * halfWidth - right * halfWidth, center + up * halfWidth + right * halfWidth,
                                           center - up * halfWidth + right * halfWidth, center - up * halfWidth - right * halfWidth };
                for (int i = 0; i < 4; ++i)
                {
                    DrawLine3D(editorCamera.position, aCorners[i], YELLOW);
                    DrawLine3D(aCorners[i], aCorners[(i + 1) % 4], YELLOW);
                }
            }
            EndMode3D();
            m_FrameTimings.m_Main3dPassMs = ElapsedMilliseconds(main3dStart);

            if (m_Editor.m_Preferences.m_ShowLightHelpers && m_Editor.m_Ui.m_Viewport.m_WindowSize.x > 0.0f &&
                m_Editor.m_Ui.m_Viewport.m_WindowSize.y > 0.0f)
            {
                const Texture2D* pLightHelper = m_Editor.m_Textures.Load("assets/img/light_helper.png");
                if (pLightHelper != nullptr)
                {
                    const Camera3D& editorCamera = m_Camera.GetCamera();
                    const Mat4 view = Mat4::lookAt(editorCamera.position, editorCamera.target, editorCamera.up);
                    const Mat4 projection = Mat4::perspective(
                        editorCamera.fovy * DEG2RAD, m_Editor.m_Ui.m_Viewport.m_WindowSize.x / m_Editor.m_Ui.m_Viewport.m_WindowSize.y, 0.1f, 1000.0f);
                    const float texW = static_cast<float>(pLightHelper->width);
                    const float texH = static_cast<float>(pLightHelper->height);
                    const float aspect = texH / texW;
                    const float helperWorldSize = 0.8f;
                    const float helperMaxPx = m_Editor.m_Ui.m_Viewport.m_WindowSize.y * 0.2f;
                    const float fovScale = tanf(editorCamera.fovy * DEG2RAD * 0.5f);

                    for (int entityIndex = 0; entityIndex < static_cast<int>(m_Editor.m_Scene.m_vEntities.size()); ++entityIndex)
                    {
                        auto& entity = m_Editor.m_Scene.m_vEntities[entityIndex];
                        CLightComponent* pLight = entity.GetLightComponent();
                        CTransformComponent* pTransform = entity.GetTransformComponent();
                        if (!pLight || !pTransform || !pLight->m_Enabled)
                        {
                            continue;
                        }
                        const Mat4& worldTransform = m_SceneRenderer.GetCachedWorldTransform(entityIndex);
                        const Vec3 worldPos = Vec3(worldTransform * Vec3{ 0.0f, 0.0f, 0.0f });

                        const Vec4 clip = projection * (view * Vec4{ worldPos.x, worldPos.y, worldPos.z, 1.0f });
                        if (clip.w <= 0.000001f)
                        {
                            continue;
                        }
                        const float nx = clip.x / clip.w * 0.5f + 0.5f;
                        const float ny = -clip.y / clip.w * 0.5f + 0.5f;
                        if (nx < -0.2f || nx > 1.2f || ny < -0.2f || ny > 1.2f)
                        {
                            continue;
                        }
                        const float sx = nx * static_cast<float>(m_Editor.m_Ui.m_Viewport.m_RenderTexture.texture.width);
                        const float sy = ny * static_cast<float>(m_Editor.m_Ui.m_Viewport.m_RenderTexture.texture.height);

                        const Vec3 toCamera = editorCamera.position - worldPos;
                        float dist = toCamera.length();
                        if (dist < 0.0001f)
                        {
                            dist = 0.0001f;
                        }
                        const float pixelsPerMeter = (m_Editor.m_Ui.m_Viewport.m_WindowSize.y * 0.5f) / (fovScale * dist);
                        float helperH = helperWorldSize * pixelsPerMeter;
                        if (helperH > helperMaxPx)
                        {
                            helperH = helperMaxPx;
                        }
                        const float helperW = helperH / aspect;

                        DrawTexturePro(*pLightHelper, { 0.0f, 0.0f, texW, texH }, { sx, sy, helperW, helperH }, { helperW * 0.5f, helperH * 0.5f }, 0.0f, WHITE);
                    }
                }
            }
        }
        {
            const CViewportState& sceneVp = m_Editor.m_Ui.m_Viewport;
            const Vec2 rawMouse = GetMousePosition();
            const float scaleX = sceneVp.m_WindowSize.x > 0.0f ? static_cast<float>(sceneVp.m_RenderTexture.texture.width) / sceneVp.m_WindowSize.x : 1.0f;
            const float scaleY = sceneVp.m_WindowSize.y > 0.0f ? static_cast<float>(sceneVp.m_RenderTexture.texture.height) / sceneVp.m_WindowSize.y : 1.0f;
            const Vec2 localMouse = { (rawMouse.x - sceneVp.m_WindowPos.x) * scaleX, (rawMouse.y - sceneVp.m_WindowPos.y) * scaleY };
            const bool sceneGuiInteractive = m_Editor.m_IsPlaying && !m_Editor.m_GameViewActive;
            const bool sceneGui2D = m_Editor.m_Scene2DMode;
            Camera2D guiCamera = {};
            if (sceneGui2D)
            {
                guiCamera.offset = { sceneVp.m_WindowSize.x * 0.5f, sceneVp.m_WindowSize.y * 0.5f };
                guiCamera.target = { m_Camera.m_Cam.target.x, m_Camera.m_Cam.target.y };
                guiCamera.zoom = m_Editor.m_Scene2DZoom;
                BeginMode2D(guiCamera);
                quark::gui::SetGuiScissorCamera2D(&guiCamera);
            }
            quark::gui::RenderSceneGui(m_Editor.m_Scene, localMouse, sceneGuiInteractive);
            if (sceneGui2D)
            {
                quark::gui::SetGuiScissorCamera2D(nullptr);
                EndMode2D();
            }
        }
        EndTextureMode();
    }

    CViewportState& gameViewport = m_Editor.m_Ui.m_GameViewport;
    if (m_Editor.m_ShowGameView && gameViewport.m_RenderTexture.id > 0 &&
        IsRenderTextureValid(gameViewport.m_RenderTexture))
    {
        const auto game3dStart = std::chrono::steady_clock::now();
        BeginTextureMode(gameViewport.m_RenderTexture);
        if (!m_Editor.m_GameCameraAvailable)
        {
            ClearBackground(Color{ 35, 35, 35, 255 });
        }
        else
        {
            ClearBackground(Color{ static_cast<unsigned char>(
                                       m_Editor.m_Preferences.m_BackgroundRed),
                                   static_cast<unsigned char>(
                                       m_Editor.m_Preferences.m_BackgroundGreen),
                                   static_cast<unsigned char>(
                                       m_Editor.m_Preferences.m_BackgroundBlue),
                                   255 });
            const float viewportWidth = gameViewport.m_WindowSize.x;
            const float viewportHeight = gameViewport.m_WindowSize.y;
            const float aspectRatio =
                viewportHeight > 0.0f ? viewportWidth / viewportHeight : 1.0f;
            BeginMode3D(gameCamera);
            for (int entityIndex = 0;
                 entityIndex <
                 static_cast<int>(m_Editor.m_Scene.m_vEntities.size());
                 ++entityIndex)
            {
                CEntity& entity = m_Editor.m_Scene.m_vEntities[entityIndex];
                CMeshComponent* pMesh = entity.GetMeshComponent();
                CTransformComponent* pTransform = entity.GetTransformComponent();
                CMaterialComponent* pMaterial = entity.GetMaterialComponent();
                if (!pMesh || !pMesh->m_Enabled || !pTransform)
                {
                    continue;
                }

                const Mat4& worldTransform =
                    m_SceneRenderer.GetCachedWorldTransform(entityIndex);
                if (!m_SceneRenderer.IsMeshVisible(entityIndex, gameCamera,
                                                   aspectRatio))
                {
                    continue;
                }

                m_SceneRenderer.EnsureLightingShader(pMesh);
                m_SceneRenderer.SetUseTexture(pMaterial != nullptr &&
                                              pMaterial->m_Texture.id != 0);
                m_SceneRenderer.BeginLightingPass();
                CEntityTextureService::DrawEntityWithTexture(
                    entity, worldTransform, m_Editor.m_Preferences);
                m_SceneRenderer.EndLightingPass();
            }
            EndMode3D();
        }
        {
            const Vec2 rawMouse = GetMousePosition();
            const float scaleX =
                gameViewport.m_WindowSize.x > 0.0f
                ? static_cast<float>(
                      gameViewport.m_RenderTexture.texture.width) /
                    gameViewport.m_WindowSize.x
                : 1.0f;
            const float scaleY =
                gameViewport.m_WindowSize.y > 0.0f
                ? static_cast<float>(
                      gameViewport.m_RenderTexture.texture.height) /
                    gameViewport.m_WindowSize.y
                : 1.0f;
            const Vec2 localMouse = {
                (rawMouse.x - gameViewport.m_WindowPos.x) * scaleX,
                (rawMouse.y - gameViewport.m_WindowPos.y) * scaleY
            };
            const bool gameGuiInteractive =
                m_Editor.m_IsPlaying && m_Editor.m_GameViewActive;
            quark::gui::RenderSceneGui(m_Editor.m_Scene, localMouse,
                                       gameGuiInteractive);
        }
        EndTextureMode();
        m_FrameTimings.m_Main3dPassMs += ElapsedMilliseconds(game3dStart);
    }

    const auto imguiStart = std::chrono::steady_clock::now();
    QcImGuiBegin();
    const bool buildSettingsOpen = m_Editor.m_BuildSettingsModalOpen;
    if (m_Editor.m_IsPlaying)
    {
        const CViewportState& activeViewport = m_Editor.m_GameViewActive
            ? m_Editor.m_Ui.m_GameViewport
            : m_Editor.m_Ui.m_Viewport;
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool anyPopupOpen = ImGui::IsPopupOpen(
            nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        const bool mouseOverActiveViewport =
            activeViewport.m_Hovered &&
            !anyPopupOpen &&
            !ImGui::IsAnyItemHovered() &&
            mousePos.x >= activeViewport.m_WindowPos.x &&
            mousePos.x <= activeViewport.m_WindowPos.x + activeViewport.m_WindowSize.x &&
            mousePos.y >= activeViewport.m_WindowPos.y &&
            mousePos.y <= activeViewport.m_WindowPos.y + activeViewport.m_WindowSize.y;

        const bool hasSceneUi = quark::gui::SceneHasActiveUiWidgets(m_Editor.m_Scene);
        if (IsKeyPressed(KEY_ESCAPE) && quark::script::CScriptEngine::Get().IsCursorLocked())
        {
            quark::script::CScriptEngine::Get().SetCursorLocked(false);
        }
        else if (!hasSceneUi &&
                 !quark::gui::CGuiSystem::Get().WantsMouseCapture() &&
                 !quark::script::CScriptEngine::Get().IsCursorLocked() &&
                 mouseOverActiveViewport && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            quark::script::CScriptEngine::Get().SetCursorLocked(true);
        }

        if (buildSettingsOpen && quark::script::CScriptEngine::Get().IsCursorLocked())
        {
            quark::script::CScriptEngine::Get().SetCursorLocked(false);
        }
        m_GameCursorCaptured = quark::script::CScriptEngine::Get().IsCursorLocked();
    }
    else
    {
        m_GameCursorCaptured = false;
    }

    if (m_Camera.m_Active && (m_Editor.m_IsPlaying || m_Editor.m_GameViewActive))
    {
        m_Camera.ReleaseCapture();
    }

    const bool scriptCursorCaptured = m_Editor.m_IsPlaying &&
        quark::script::CScriptEngine::Get().IsCursorLocked();

    ImGuiIO& io = ImGui::GetIO();
    if (scriptCursorCaptured)
    {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
        if (!IsCursorHidden())
        {
            DisableCursor();
        }
        if (SDL_Window* pWin = GetNativeWindow())
        {
            if (!SDL_GetWindowRelativeMouseMode(pWin))
            {
                SDL_SetWindowRelativeMouseMode(pWin, true);
            }
        }
    }
    else
    {
        io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse);
    }

    if (!scriptCursorCaptured && !m_Camera.m_Active)
    {
        if (SDL_Window* pWindow = GetNativeWindow())
        {
            if (SDL_GetWindowMouseRect(pWindow) != nullptr &&
                !SDL_SetWindowMouseRect(pWindow, nullptr))
            {
                TraceLog(LogLevel::Error, "APPLICATION", TextFormat("Failed to release mouse confinement: %s", SDL_GetError()));
            }
        }
    }
    if (!m_Editor.m_IsPlaying && !m_Editor.m_GameViewActive)
    {
        m_Camera.ApplyActiveCursor();
    }
    m_Editor.HandleInput();

    m_Editor.DrawUi(m_SceneRenderer.GetLightingShader(), m_Camera, m_PluginBridge.GetContext());

    if (m_Camera.m_Active &&
        (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ||
         m_Editor.m_BuildSettingsModalOpen))
    {
        m_Camera.ReleaseCapture();
    }

    if (SDL_Window* pWindow = GetNativeWindow())
    {
        const bool editorCameraCaptured =
            !m_Editor.m_IsPlaying && !m_Editor.m_GameViewActive &&
            m_Camera.m_Active;
        if (scriptCursorCaptured || editorCameraCaptured)
        {
            const CViewportState& viewport = (m_Editor.m_IsPlaying && !m_Editor.m_GameViewActive)
                ? m_Editor.m_Ui.m_Viewport
                : (scriptCursorCaptured ? m_Editor.m_Ui.m_GameViewport : m_Editor.m_Ui.m_Viewport);
            const ImGuiViewport* pMainViewport = ImGui::GetMainViewport();
            const SDL_Rect mouseRect = {
                static_cast<int>(viewport.m_WindowPos.x - pMainViewport->Pos.x),
                static_cast<int>(viewport.m_WindowPos.y - pMainViewport->Pos.y),
                static_cast<int>(viewport.m_WindowSize.x),
                static_cast<int>(viewport.m_WindowSize.y)
            };
            const SDL_Rect* pCurrentMouseRect = SDL_GetWindowMouseRect(pWindow);
            if (mouseRect.w > 0 && mouseRect.h > 0 &&
                (pCurrentMouseRect == nullptr ||
                 pCurrentMouseRect->x != mouseRect.x ||
                 pCurrentMouseRect->y != mouseRect.y ||
                 pCurrentMouseRect->w != mouseRect.w ||
                 pCurrentMouseRect->h != mouseRect.h) &&
                !SDL_SetWindowMouseRect(pWindow, &mouseRect))
            {
                TraceLog(LogLevel::Error, "APPLICATION", TextFormat("Failed to confine mouse for Play mode: %s", SDL_GetError()));
            }
        }
    }

    const SEditorUiTimings editorUiTimings = m_Editor.GetUiTimings();
    m_FrameTimings.m_HierarchyMs = editorUiTimings.m_HierarchyMs;
    m_FrameTimings.m_InspectorMs = editorUiTimings.m_InspectorMs;
    m_FrameTimings.m_ScenePanelMs = editorUiTimings.m_ScenePanelMs;
    m_FrameTimings.m_AssetsMs = editorUiTimings.m_AssetsMs;
    m_FrameTimings.m_ModelViewerMs = editorUiTimings.m_ModelViewerMs;

    if (!m_Editor.IsHistoryRestorePending())
    {
        const auto pluginUpdateStart = std::chrono::steady_clock::now();
        m_PluginBridge.Update(m_Editor, m_PluginManager);
        m_FrameTimings.m_PluginUpdateMs = ElapsedMilliseconds(pluginUpdateStart);
    }

    QcImGuiEnd();
    m_FrameTimings.m_ImguiEditorMs = ElapsedMilliseconds(imguiStart);
    m_Editor.m_CpuTaskPool.RecordMainThreadBusy(
        CTaskPool::CurrentThreadCpuTime() - renderStart);
    m_Editor.m_CpuTaskPool.SetMainThreadTask("");
    const auto endDrawingStart = std::chrono::steady_clock::now();
    EndDrawing();
    m_FrameTimings.m_EndDrawingMs = ElapsedMilliseconds(endDrawingStart);
    m_PluginBridge.SetFrameTimings(m_FrameTimings);
}

void CApplication::Run()
{
    if (!m_ReadyToRun)
    {
        return;
    }

    constexpr auto USAGE_SAMPLE_INTERVAL = std::chrono::milliseconds(100);
    auto lastUsageSample = std::chrono::steady_clock::now();
    while (!WindowShouldClose())
    {
        m_Editor.m_CpuTaskPool.SetMainThreadTask("Update editor and scene");
        const auto updateFrameStart = std::chrono::steady_clock::now();
        const auto frameStart = CTaskPool::CurrentThreadCpuTime();
        UpdateFrame();
        m_FrameTimings.m_UpdateFrameMs = ElapsedMilliseconds(updateFrameStart);
        const SSceneRendererTimings rendererTimings = m_SceneRenderer.GetTimings();
        m_FrameTimings.m_SceneRendererUpdateMs = rendererTimings.m_UpdateMs;
        m_FrameTimings.m_ShadowPassMs = rendererTimings.m_ShadowPassMs;
        m_Editor.m_CpuTaskPool.RecordMainThreadBusy(
            CTaskPool::CurrentThreadCpuTime() - frameStart);
        m_Editor.m_CpuTaskPool.SetMainThreadTask("");
        RenderFrame();

        const auto sampleTime = std::chrono::steady_clock::now();
        const auto sampleElapsed = sampleTime - lastUsageSample;
        if (sampleElapsed >= USAGE_SAMPLE_INTERVAL)
        {
            m_Editor.m_CpuTaskPool.SampleUsage(
                std::chrono::duration_cast<std::chrono::nanoseconds>(sampleElapsed));
            lastUsageSample = sampleTime;
        }
    }
}

void CApplication::Shutdown()
{
    if (!m_ReadyToRun)
    {
        return;
    }

    if (!m_Editor.m_Scene2DMode)
    {
        m_Editor.m_Preferences.m_CameraFov = m_Camera.GetCamera().fovy;
    }
    m_Editor.m_Preferences.Save();

    Unload();
}

void CApplication::Unload()
{
    m_ReadyToRun = false;

    m_PluginManager.UnloadAll();
    m_Editor.Unload();
    m_SceneRenderer.Unload();
    quark::gui::ShutdownGuiBackend();
    QcImGuiShutdown();

    m_PluginBridge.Reset();
    m_Editor.m_pPluginManager = nullptr;

    if (m_WindowOpen)
    {
        m_WindowOpen = false;
        CloseWindow();
    }
}
