#include "camera.h"
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "ImGuizmo.h"
#include "editor/editor_preferences.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_surface.h"

namespace
{

constexpr int CAMERA_CURSOR_SIZE = 32;

void SetCameraCapture(bool enabled)
{
    if (SDL_Window* pWindow = GetNativeWindow())
    {
        SDL_SetWindowRelativeMouseMode(pWindow, enabled);
    }

    if (enabled)
    {
        DisableCursor();
    }
    else
    {
        EnableCursor();
    }
}

void SetOrbitCursor(SDL_Cursor* pCursor)
{
    if (SDL_Window* pWindow = GetNativeWindow())
    {
        if (!SDL_SetWindowRelativeMouseMode(pWindow, false))
        {
            TraceLog(LogLevel::Error, "CAMERA", TextFormat(
                "Failed to disable relative mouse mode: %s", SDL_GetError()));
        }
    }

    EnableCursor();
    if (pCursor != nullptr && !SDL_SetCursor(pCursor))
    {
        TraceLog(LogLevel::Error, "CAMERA", TextFormat(
            "Failed to set camera cursor: %s", SDL_GetError()));
    }
}

} // anonymous

CFlyCamera::CFlyCamera()
{
    m_Cam.position = {5.0f, 5.0f, 5.0f};
    m_Cam.target = {0.0f, 0.0f, 0.0f};
    m_Cam.up = {0.0f, 1.0f, 0.0f};
    m_Cam.fovy = 45.0f;
    m_Cam.projection = CAMERA_PERSPECTIVE;

    Vec3 direction = m_Cam.target - m_Cam.position;
    m_Yaw = atan2f(direction.x, direction.z);
    m_Pitch = asinf(direction.y / sqrtf(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z));

}

void CFlyCamera::InitializeCursors()
{
    if (m_pOrbitCursor != nullptr || m_pPanCursor != nullptr)
    {
        return;
    }

    Image eyeImage = LoadImage("assets/img/eye_icon.png");
    if (eyeImage.data != nullptr)
    {
        ImageFormat(&eyeImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageResize(&eyeImage, CAMERA_CURSOR_SIZE, CAMERA_CURSOR_SIZE);
        SDL_Surface* pSurface = SDL_CreateSurfaceFrom(
            eyeImage.width,
            eyeImage.height,
            SDL_PIXELFORMAT_RGBA32,
            eyeImage.data,
            eyeImage.width * 4);
        if (pSurface != nullptr)
        {
            m_pOrbitCursor = SDL_CreateColorCursor(
                pSurface,
                CAMERA_CURSOR_SIZE / 2,
                CAMERA_CURSOR_SIZE / 2);
            SDL_DestroySurface(pSurface);
        }
        UnloadImage(eyeImage);
    }

    if (m_pOrbitCursor == nullptr)
    {
        TraceLog(LogLevel::Error, "CAMERA", TextFormat(
            "Failed to create orbit cursor from assets/img/eye_icon.png: %s", SDL_GetError()));
    }

    m_pPanCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    if (m_pPanCursor == nullptr)
    {
        TraceLog(LogLevel::Error, "CAMERA", TextFormat(
            "Failed to create pan cursor: %s", SDL_GetError()));
    }
}

CFlyCamera::~CFlyCamera()
{
    if (m_ImGuiCursorOverride && ImGui::GetCurrentContext() != nullptr)
    {
        SetImGuiCursorOverride(false);
    }

    const SDL_Cursor* pCurrentCursor = SDL_GetCursor();
    if ((m_pOrbitCursor != nullptr && pCurrentCursor == m_pOrbitCursor) ||
        (m_pPanCursor != nullptr && pCurrentCursor == m_pPanCursor))
    {
        SDL_SetCursor(SDL_GetDefaultCursor());
    }

    if (m_pOrbitCursor != nullptr)
    {
        SDL_DestroyCursor(m_pOrbitCursor);
    }
    if (m_pPanCursor != nullptr)
    {
        SDL_DestroyCursor(m_pPanCursor);
    }
}

void CFlyCamera::Update(CScene& scene, CPreferences& preferences)
{
    if (!m_Active && ImGuizmo::IsUsing())
    {
        return;
    }

    CEntity* pSelected = scene.GetSelected();
    CMeshComponent* pSelectedMesh = pSelected ? pSelected->GetMeshComponent() : nullptr;
    if (pSelectedMesh && pSelectedMesh->m_VertexGizmo)
    {
        return;
    }

    const bool orbitCamera = preferences.m_OrbitCamera;
    const float wheel = GetMouseWheelMove();
    if (orbitCamera)
    {
        UpdateOrbitCamera(wheel);
    }
    else if (fabsf(wheel) > 0.001f)
    {
        m_Cam.fovy -= wheel * m_ZoomSensitivity;
        m_Cam.fovy = std::clamp(m_Cam.fovy, 20.0f, 120.0f);
        preferences.m_CameraFov = m_Cam.fovy;
    }

    if (orbitCamera)
    {
        if (!m_Active && !ImGuizmo::IsOver() && !ImGui::IsAnyItemActive())
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                m_Active = true;
                m_ActiveMouseControl = EMouseControl::ORBIT;
                SetImGuiCursorOverride(true);
                SetOrbitCursor(m_pOrbitCursor);
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                m_Active = true;
                m_ActiveMouseControl = EMouseControl::PAN;
                SetImGuiCursorOverride(true);
                SetOrbitCursor(m_pPanCursor);
            }

            if (m_Active)
            {
                SDL_GetMouseState(&m_LastMouseX, &m_LastMouseY);
            }
        }

        const bool activeButtonDown =
            (m_ActiveMouseControl == EMouseControl::ORBIT && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) ||
            (m_ActiveMouseControl == EMouseControl::PAN && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
        if (IsKeyPressed(KEY_ESCAPE) || !IsWindowFocused() || (m_Active && !activeButtonDown))
        {
            SetImGuiCursorOverride(false);
            SetCameraCapture(false);
            if (!SDL_SetCursor(SDL_GetDefaultCursor()))
            {
                TraceLog(LogLevel::Error, "CAMERA", TextFormat(
                    "Failed to restore default cursor: %s", SDL_GetError()));
            }
            m_Active = false;
            m_ActiveMouseControl = EMouseControl::NONE;
        }
    }
    else
    {
        const MouseButton cameraButton = MOUSE_BUTTON_LEFT;
        if (IsMouseButtonPressed(cameraButton) &&
            !ImGuizmo::IsOver() && !ImGui::IsAnyItemActive())
        {
            SetImGuiCursorOverride(false);
            SetCameraCapture(true);
            m_Active = true;
        }

        if (IsKeyPressed(KEY_ESCAPE) || !IsWindowFocused())
        {
            SetImGuiCursorOverride(false);
            SetCameraCapture(false);
            m_Active = false;
            m_ActiveMouseControl = EMouseControl::NONE;
        }
    }

    float relativeX = 0.0f;
    float relativeY = 0.0f;
    if (orbitCamera && m_Active)
    {
        SDL_GetMouseState(&relativeX, &relativeY);
        const float mouseX = relativeX;
        const float mouseY = relativeY;
        relativeX = mouseX - m_LastMouseX;
        relativeY = mouseY - m_LastMouseY;
        m_LastMouseX = mouseX;
        m_LastMouseY = mouseY;
    }
    else if (m_Active)
    {
        SDL_GetRelativeMouseState(&relativeX, &relativeY);
    }
    else
    {
        return;
    }

    Vec2 mouseDelta = {relativeX, relativeY};
    if (fabs(mouseDelta.x) > 100 || fabs(mouseDelta.y) > 100)
    {
        mouseDelta = {0, 0};
    }

    const float deltaTime = GetDeltaTime();
    Vec3 forward =
    {
        cosf(m_Pitch) * sinf(m_Yaw),
        sinf(m_Pitch),
        cosf(m_Pitch) * cosf(m_Yaw),
    };

    forward.normalized();
    if (orbitCamera)
    {
        if (m_ActiveMouseControl == EMouseControl::ORBIT)
        {
            m_Yaw -= mouseDelta.x * m_Sensitivity;
            m_Pitch = std::clamp(m_Pitch - mouseDelta.y * m_Sensitivity, -1.5f, 1.5f);

            forward =
            {
                cosf(m_Pitch) * sinf(m_Yaw),
                sinf(m_Pitch),
                cosf(m_Pitch) * cosf(m_Yaw),
            };
            forward.normalized();

            const float distance = (m_Cam.target - m_Cam.position).length();
            m_Cam.position = m_Cam.target - (forward * distance);
        }
        else if (m_ActiveMouseControl == EMouseControl::PAN)
        {
            Vec3 right = forward.cross(m_Cam.up).normalized();
            Vec3 cameraUp = right.cross(forward).normalized();
            const float panScale = (m_Cam.target - m_Cam.position).length() * m_Sensitivity;
            const Vec3 panOffset =
                (right * (-mouseDelta.x * panScale)) +
                (cameraUp * (mouseDelta.y * panScale));
            m_Cam.position = m_Cam.position + panOffset;
            m_Cam.target = m_Cam.target + panOffset;
        }
    }
    else
    {
        m_Yaw -= mouseDelta.x * m_Sensitivity;
        m_Pitch = std::clamp(m_Pitch - mouseDelta.y * m_Sensitivity, -1.5f, 1.5f);

        forward =
        {
            cosf(m_Pitch) * sinf(m_Yaw),
            sinf(m_Pitch),
            cosf(m_Pitch) * cosf(m_Yaw),
        };
        forward.normalized();

        const Vec3 right = {sinf(m_Yaw - PI / 2), 0, cosf(m_Yaw - PI / 2)};
        if (IsKeyDown(KEY_W))
        {
            m_Cam.position = m_Cam.position + (forward * (m_Speed * deltaTime));
        }
        if (IsKeyDown(KEY_S))
        {
            m_Cam.position = m_Cam.position - (forward * (m_Speed * deltaTime));
        }
        if (IsKeyDown(KEY_A))
        {
            m_Cam.position = m_Cam.position - (right * (m_Speed * deltaTime));
        }
        if (IsKeyDown(KEY_D))
        {
            m_Cam.position = m_Cam.position + (right * (m_Speed * deltaTime));
        }

        m_Cam.target = m_Cam.position + forward;
    }
}

void CFlyCamera::ApplyActiveCursor()
{
    if (!m_Active || m_ActiveMouseControl == EMouseControl::NONE)
    {
        return;
    }

    SDL_Cursor* pCursor = m_ActiveMouseControl == EMouseControl::ORBIT
        ? m_pOrbitCursor
        : m_pPanCursor;
    if (pCursor == nullptr)
    {
        return;
    }

    EnableCursor();
    if (!SDL_SetCursor(pCursor))
    {
        TraceLog(LogLevel::Error, "CAMERA", TextFormat(
            "Failed to reapply camera cursor: %s", SDL_GetError()));
    }
}

void CFlyCamera::UpdateOrbitCamera(float wheel)
{
    if (fabsf(wheel) <= 0.001f)
    {
        return;
    }

    Vec3 offset = m_Cam.position - m_Cam.target;
    const float distance = offset.length();
    if (distance <= 0.001f)
    {
        return;
    }

    const float zoomFactor = expf(-wheel * m_ZoomSensitivity * 0.1f);
    const float newDistance = std::clamp(distance * zoomFactor, 0.1f, 10000.0f);
    m_Cam.position = m_Cam.target + (offset * (newDistance / distance));
}

void CFlyCamera::SetImGuiCursorOverride(bool enabled)
{
    if (enabled == m_ImGuiCursorOverride)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (enabled)
    {
        m_PreviousImGuiCursorChangeDisabled =
            (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
    else
    {
        if (m_PreviousImGuiCursorChangeDisabled)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        }
        else
        {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        }
    }

    m_ImGuiCursorOverride = enabled;
}

void CFlyCamera::FocusOn(const Vec3& point)
{
    m_Cam.position = point + Vec3{5.0f, 5.0f, 5.0f};
    m_Cam.target = point;

    Vec3 direction = m_Cam.target - m_Cam.position;
    m_Yaw = atan2f(direction.x, direction.z);
    m_Pitch = asinf(direction.y / sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z));
}

Camera3D& CFlyCamera::GetCamera()
{
    return m_Cam;
}
