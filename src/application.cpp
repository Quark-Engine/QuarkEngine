#include "application.h"
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
#include <cfloat>
#include <iostream>
#include <SDL3/SDL_video.h>

using namespace qc;

namespace fs = std::filesystem;

static bool LanguageUsesMsPgothic(const std::string& languageCode)
{
    return languageCode == "japanese" ||
        languageCode == "korean" ||
        languageCode == "simplified_chinese" ||
        languageCode == "traditional_chinese";
}

static const ImWchar* GetMsPgothicGlyphRanges(ImGuiIO& io, const std::string& languageCode)
{
    if (languageCode == "japanese")
    {
        return io.Fonts->GetGlyphRangesJapanese();
    }
    if (languageCode == "korean")
    {
        return io.Fonts->GetGlyphRangesKorean();
    }
    if (languageCode == "simplified_chinese")
    {
        return io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    }
    if (languageCode == "traditional_chinese")
    {
        return io.Fonts->GetGlyphRangesChineseFull();
    }
    return nullptr;
}

static void ReloadEditorFonts(const std::string& languageCode)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const std::string baseFontPath = CLanguageManager::Get().EditorFontPath();
    const std::string mergeFontPath = CLanguageManager::Get().EditorFontMergePath();
    ImFont* pDefaultFont = io.Fonts->AddFontFromFileTTF(baseFontPath.c_str(), 16.0f);

    if (!mergeFontPath.empty())
    {
        ImFontConfig mergeConfig = {};
        mergeConfig.MergeMode = true;
        mergeConfig.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            mergeFontPath.c_str(),
            16.0f,
            &mergeConfig,
            nullptr
        );
    }
    else if (LanguageUsesMsPgothic(languageCode) && baseFontPath != "assets/MS-Pgothic-Regular.ttf")
    {
        ImFontConfig mergeConfig = {};
        mergeConfig.MergeMode = true;
        mergeConfig.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            "assets/MS-Pgothic-Regular.ttf",
            16.0f,
            &mergeConfig,
            GetMsPgothicGlyphRanges(io, languageCode)
        );
    }

    io.FontDefault = pDefaultFont;
    io.Fonts->Build();
}

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
    if (m_Options.HelpRequested || m_Options.VersionRequested)
    {
        return;
    }

    fs::create_directories("projects");
    fs::create_directories("assets");

    if (m_Options.TestMode)
    {
        return;
    }

    ApplyLogLevel(m_Options.LogLevel);

    std::string langCode = LoadOrCreateConfig();
    if (!m_Options.LangOverride.empty())
    {
        langCode = m_Options.LangOverride;
    }
    m_Editor.m_Preferences.Load();
    CLanguageManager::Get().SetLang(langCode);

    RendererType rendererType = RendererType::OpenGL;
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
    if (m_Options.RendererOverride == ERendererOverride::OPENGL)
    {
        rendererType = RendererType::OpenGL;
    }
    else if (m_Options.RendererOverride == ERendererOverride::VULKAN)
    {
        rendererType = RendererType::Vulkan;
    }

    SetMSAASamples(m_Editor.m_Preferences.m_MsaaSamples);
    SetTextureFilterMode(m_Editor.m_Preferences.m_TextureFilter == 0
        ? TextureFilterMode::Nearest : TextureFilterMode::Linear);

    InitWindow(1280, 720, "Quark Engine", rendererType);
    m_WindowOpen = true;

    if (m_Options.FpsOverride >= 0)
    {
        SetTargetFPS(m_Options.FpsOverride);
    }
    else
    {
        if (m_Editor.m_Preferences.m_TargetFps <= 0)
        {
            m_Editor.m_Preferences.m_TargetFps = static_cast<int>(GetCurrentMonitorRefreshRate());
        }
        SetTargetFPS(m_Editor.m_Preferences.m_LimitFps ? m_Editor.m_Preferences.m_TargetFps : 0);
    }

    const bool vsyncEnabled = m_Options.VsyncOverride == ETriState::ON ? true
        : m_Options.VsyncOverride == ETriState::OFF ? false
        : m_Editor.m_Preferences.m_VsyncEnabled;
    if (!vsyncEnabled)
    {
        SDL_GL_SetSwapInterval(0);
    }

    SetExitKey(KEY_NULL);

    m_Headless = m_Options.Headless;

    if (!m_Headless)
    {
        m_Editor.m_Text.Init();
        QcImGuiSetup(false);
        ReloadEditorFonts(CLanguageManager::Get().m_Current);
        CThemeManager::Apply(m_Editor.m_Preferences.m_LightTheme);
        ImGui::GetStyle().ScaleAllSizes(m_Editor.m_Preferences.m_InterfaceScale);
        ImGui::GetStyle().FontScaleMain = m_Editor.m_Preferences.m_InterfaceScale;
        if (ImGui::GetStyle().WindowBorderHoverPadding <= 0.0f)
        {
            ImGui::GetStyle().WindowBorderHoverPadding = 1.0f;
        }
        if (ImGui::GetStyle().SeparatorSize <= 0.0f)
        {
            ImGui::GetStyle().SeparatorSize = 1.0f;
        }
    }

    m_ProjectPath = m_Options.ProjectPath;

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

    const bool vulkanBackend = GetCurrentBackend() == RendererType::Vulkan;
    m_SceneRenderer.Initialize(vulkanBackend,
        m_Editor.m_Preferences.m_ShadowMapSize,
        m_Editor.m_Preferences.m_ShadowsEnabled,
        m_Editor.m_Preferences.m_ShadowBias,
        m_Editor.m_Preferences.m_ShadowFilterQuality);

    m_PluginBridge.Initialize(m_Editor, m_PluginManager);

    m_Editor.m_Assets.Load(m_ProjectPath);
    m_Editor.m_Assets.Refresh(m_ProjectPath, m_Editor.m_Scene);

    if (CProjectService::IsValid(m_ProjectPath) && !m_Options.NewProject)
    {
        CProjectService::Load(m_ProjectPath, m_Editor.m_Scene, m_Editor.m_Assets, m_Editor.m_Lights, m_Editor.m_ComponentFactories);
    }
    else
    {
        CProjectService::CreateNew(m_ProjectPath, m_Editor.m_Scene);
    }

    if (m_Headless)
    {
        Unload();
        return;
    }

    if (!m_Options.NoPlugins)
    {
        m_PluginManager.LoadAll(m_Options.PluginsDir.c_str(), m_PluginBridge.GetContext());
    }
    m_Editor.m_pPluginManager = &m_PluginManager;

    m_ActiveFontLanguage = CLanguageManager::Get().m_Current;
    m_LastAutosaveTime = GetTime();
    m_LastSelectedEntity = m_Editor.m_Scene.m_Selected;

    m_ReadyToRun = true;
}

void CApplication::UpdateFrame()
{
    if (m_ActiveFontLanguage != CLanguageManager::Get().m_Current)
    {
        m_ActiveFontLanguage = CLanguageManager::Get().m_Current;
        ReloadEditorFonts(m_ActiveFontLanguage);
    }

    m_Camera.m_Speed = m_Editor.m_Preferences.m_CameraSpeed;
    m_Camera.m_Sensitivity = m_Editor.m_Preferences.m_CameraSensitivity;
    m_Camera.m_ZoomSensitivity = m_Editor.m_Preferences.m_CameraZoomSensitivity;
    if (m_Editor.m_Preferences.m_FocusOnSelection && m_Editor.m_Scene.m_Selected != m_LastSelectedEntity)
    {
        CEntity* pSelectedEntity = m_Editor.m_Scene.GetSelected();
        CTransformComponent* pSelectedTransform = pSelectedEntity ? pSelectedEntity->GetTransformComponent() : nullptr;
        if (pSelectedTransform)
        {
            const int selectedIndex = m_Editor.m_Scene.m_Selected;
            const Vec3 worldPosition = Vec3Transform(
                {0.0f, 0.0f, 0.0f},
                quark::ComposeWorld(m_Editor.m_Scene, selectedIndex)
            );
            m_Camera.FocusOn(worldPosition);
        }
    }
    m_LastSelectedEntity = m_Editor.m_Scene.m_Selected;

    if (!m_Options.NoAutosave && m_Editor.m_Preferences.m_AutosaveEnabled &&
        GetTime() - m_LastAutosaveTime >= m_Editor.m_Preferences.m_AutosaveIntervalMinutes * 60.0)
        {
        if (m_Editor.m_Preferences.m_AutosaveBackupEnabled)
        {
            std::error_code backupError;
            fs::copy_file(
                fs::path(m_Editor.m_ProjectPath) / "scene.json",
                fs::path(m_Editor.m_ProjectPath) / "scene.json.bak",
                fs::copy_options::overwrite_existing,
                backupError
            );
        }
        CProjectService::Save(m_Editor.m_ProjectPath, m_Editor.m_Scene);
        m_LastAutosaveTime = GetTime();
    }

    const std::string projectTitle = fs::path(m_Editor.m_ProjectPath).filename().string();
    SetWindowTitle(TextFormat("Quark Engine | %s%s | FPS: %d",
        projectTitle.c_str(), m_Editor.m_SceneDirty ? "*" : "", GetFPS()));

    m_SceneRenderer.Update(m_Editor.m_Scene, m_Editor.m_Lights, m_Camera.GetCamera().position,
        m_Editor.m_Preferences.m_ShadowsEnabled,
        m_Editor.m_Preferences.m_ShadowBias,
        m_Editor.m_Preferences.m_ShadowFilterQuality);
}

void CApplication::RenderFrame()
{
    BeginDrawing();
        ClearBackground(DARKGRAY);

        if (m_Editor.m_Ui.m_Viewport.m_RenderTexture.id > 0 && IsRenderTextureValid(m_Editor.m_Ui.m_Viewport.m_RenderTexture))
        {
            BeginTextureMode(m_Editor.m_Ui.m_Viewport.m_RenderTexture);
            ClearBackground(Color
            {
                static_cast<unsigned char>(m_Editor.m_Preferences.m_BackgroundRed),
                static_cast<unsigned char>(m_Editor.m_Preferences.m_BackgroundGreen),
                static_cast<unsigned char>(m_Editor.m_Preferences.m_BackgroundBlue),
                255
            });
            BeginMode3D(m_Camera.GetCamera());
                if (m_Editor.m_Preferences.m_ShowGrid)
                {
                    CInfiniteGrid::Draw(
                        m_Camera.GetCamera(),
                        (int)m_Editor.m_Ui.m_Viewport.m_WindowSize.x,
                        (int)m_Editor.m_Ui.m_Viewport.m_WindowSize.y,
                        1.0f,
                        GRAY
                    );
                }
                if (m_Editor.m_Preferences.m_ShowAxes)
                {
                    DrawLine3D({0, 0, 0}, {3, 0, 0}, RED);
                    DrawLine3D({0, 0, 0}, {0, 3, 0}, GREEN);
                    DrawLine3D({0, 0, 0}, {0, 0, 3}, BLUE);
                }
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

                    m_SceneRenderer.EnsureLightingShader(pMesh);

                    int use = (pMat && pMat->m_Texture.id != 0) ? 1 : 0;
                    m_SceneRenderer.SetUseTexture(use != 0);
                    CEntityTextureService::DrawEntityWithTexture(e, quark::ComposeWorld(m_Editor.m_Scene, entityIndex), m_Editor.m_Preferences);
                    if (m_Editor.m_Preferences.m_ShowSelectionVisualization &&
                        m_Editor.m_Scene.IsSelected(entityIndex) && pMesh->m_Model.meshCount > 0)
                        {
                        PushMatrix();
                        MultMatrix(quark::ComposeMeshWorld(m_Editor.m_Scene, e));
                        const bool primarySelection = entityIndex == m_Editor.m_Scene.m_Selected;
                        DrawBoundingBox(GetModelBoundingBox(pMesh->m_Model), Color
                        {
                            static_cast<unsigned char>(m_Editor.m_Preferences.m_SelectionRed),
                            static_cast<unsigned char>(m_Editor.m_Preferences.m_SelectionGreen),
                            static_cast<unsigned char>(m_Editor.m_Preferences.m_SelectionBlue),
                            static_cast<unsigned char>(primarySelection ? 255 : 150)
                        });
                        PopMatrix();
                    }
                    if (m_Editor.m_Preferences.m_ShowBoundingBoxes && pMesh->m_Model.meshCount > 0)
                    {
                        PushMatrix();
                        MultMatrix(quark::ComposeMeshWorld(m_Editor.m_Scene, e));
                        DrawBoundingBox(GetModelBoundingBox(pMesh->m_Model), Color
                        {
                            static_cast<unsigned char>(m_Editor.m_Preferences.m_BoundsRed),
                            static_cast<unsigned char>(m_Editor.m_Preferences.m_BoundsGreen),
                            static_cast<unsigned char>(m_Editor.m_Preferences.m_BoundsBlue), 255
                        });
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
                        const Mat4 worldTransform = quark::ComposeWorld(m_Editor.m_Scene, entityIndex);
                        const Vec3 worldPosition = Vec3(worldTransform * Vec3{0.0f, 0.0f, 0.0f});
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
                    const Vec3 aCorners[4] = {
                        center + up * halfWidth - right * halfWidth,
                        center + up * halfWidth + right * halfWidth,
                        center - up * halfWidth + right * halfWidth,
                        center - up * halfWidth - right * halfWidth
                    };
                    for (int i = 0; i < 4; ++i)
                    {
                        DrawLine3D(editorCamera.position, aCorners[i], YELLOW);
                        DrawLine3D(aCorners[i], aCorners[(i + 1) % 4], YELLOW);
                    }
                }
            EndMode3D();

            if (m_Editor.m_Preferences.m_ShowLightHelpers && m_Editor.m_Ui.m_Viewport.m_WindowSize.x > 0.0f && m_Editor.m_Ui.m_Viewport.m_WindowSize.y > 0.0f)
            {
                const Texture2D* pLightHelper = m_Editor.m_Textures.Load("assets/light_helper.png");
                if (pLightHelper != nullptr)
                {
                    const Camera3D& editorCamera = m_Camera.GetCamera();
                    const Mat4 view = Mat4::lookAt(editorCamera.position, editorCamera.target, editorCamera.up);
                    const Mat4 projection = Mat4::perspective(
                        editorCamera.fovy * DEG2RAD,
                        m_Editor.m_Ui.m_Viewport.m_WindowSize.x / m_Editor.m_Ui.m_Viewport.m_WindowSize.y,
                        0.1f,
                        1000.0f
                    );
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
                        const Mat4 worldTransform = quark::ComposeWorld(m_Editor.m_Scene, entityIndex);
                        const Vec3 worldPos = Vec3(worldTransform * Vec3{0.0f, 0.0f, 0.0f});

                        const Vec4 clip = projection * (view * Vec4{worldPos.x, worldPos.y, worldPos.z, 1.0f});
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

                        DrawTexturePro(
                            *pLightHelper,
                            {0.0f, 0.0f, texW, texH},
                            {sx, sy, helperW, helperH},
                            {helperW * 0.5f, helperH * 0.5f},
                            0.0f,
                            WHITE);
                    }
                }
            }
            EndTextureMode();
        }

        QcImGuiBegin();

        const bool gizmoBusy = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
        if (!gizmoBusy && (IsCursorHidden() || m_Editor.m_Ui.m_Viewport.m_Hovered))
        {
            if (SDL_Window* pWindow = GetNativeWindow())
            {
                if (m_Camera.m_Active && m_Editor.m_Ui.m_Viewport.m_WindowSize.x > 0.0f && m_Editor.m_Ui.m_Viewport.m_WindowSize.y > 0.0f)
                {
                    const SDL_Rect sceneMouseRect = {
                        static_cast<int>(m_Editor.m_Ui.m_Viewport.m_WindowPos.x),
                        static_cast<int>(m_Editor.m_Ui.m_Viewport.m_WindowPos.y),
                        static_cast<int>(m_Editor.m_Ui.m_Viewport.m_WindowSize.x),
                        static_cast<int>(m_Editor.m_Ui.m_Viewport.m_WindowSize.y)
                    };
                    SDL_SetWindowMouseRect(pWindow, &sceneMouseRect);
                }
                else if (!m_Camera.m_Active)
                {
                    SDL_SetWindowMouseRect(pWindow, nullptr);
                }
            }
            m_Camera.Update(m_Editor.m_Scene, m_Editor.m_Preferences);
        }

        m_Editor.HandleInput();

        m_Editor.DrawUi(m_SceneRenderer.GetLightingShader(), m_Camera, m_PluginBridge.GetContext());

        m_PluginBridge.Update(m_Editor, m_PluginManager);

        QcImGuiEnd();
    EndDrawing();
}

void CApplication::Run()
{
    if (!m_ReadyToRun)
    {
        return;
    }

    while (!WindowShouldClose())
    {
        UpdateFrame();
        RenderFrame();
    }
}

void CApplication::Shutdown()
{
    if (!m_ReadyToRun)
    {
        return;
    }

    m_Editor.m_Preferences.m_CameraFov = m_Camera.GetCamera().fovy;
    m_Editor.m_Preferences.Save();

    Unload();
}

void CApplication::Unload()
{
    m_ReadyToRun = false;

    m_PluginManager.UnloadAll();
    m_Editor.Unload();
    m_SceneRenderer.Unload();
    QcImGuiShutdown();

    m_PluginBridge.Reset();
    m_Editor.m_pPluginManager = nullptr;

    if (m_WindowOpen)
    {
        m_WindowOpen = false;
        CloseWindow();
    }
}
