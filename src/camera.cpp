#include "camera.h"
#include <cmath>
#include "imgui.h"
#include "ImGuizmo.h"
#include "editor/editor_preferences.h"
#include "SDL3/SDL_mouse.h"

using namespace qc;

namespace
{

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

void CFlyCamera::Update(CScene& scene, CPreferences& preferences)
{
    if (!m_Active && ImGuizmo::IsUsing())
        return;

    CEntity* pSelected = scene.GetSelected();
    CMeshComponent* pSelectedMesh = pSelected ? pSelected->GetMeshComponent() : nullptr;
    if (pSelectedMesh && pSelectedMesh->m_VertexGizmo) return;

    const float wheel = GetMouseWheelMove();
    if (fabsf(wheel) > 0.001f)
    {
        m_Cam.fovy -= wheel * m_ZoomSensitivity;
        if (m_Cam.fovy < 20.0f) m_Cam.fovy = 20.0f;
        if (m_Cam.fovy > 120.0f) m_Cam.fovy = 120.0f;
        preferences.m_CameraFov = m_Cam.fovy;
    }

    if (IsMouseButtonPressed(MouseButton::Left) &&
        !ImGuizmo::IsOver() && !ImGui::IsAnyItemActive())
    {
        SetCameraCapture(true);
        m_Active = true;
    }

    if (IsKeyPressed(KeyboardKey::Escape) || !IsWindowFocused())
    {
        SetCameraCapture(false);
        m_Active = false;
    }

    if (!m_Active) return;

    float dt = GetDeltaTime();
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    SDL_GetRelativeMouseState(&relativeX, &relativeY);
    Vec2 mouseDelta = { relativeX, relativeY };
    if (fabs(mouseDelta.x) > 100 || fabs(mouseDelta.y) > 100) mouseDelta = {0,0};

    m_Yaw   -= mouseDelta.x * m_Sensitivity;
    m_Pitch -= mouseDelta.y * m_Sensitivity;

    if (m_Pitch > 1.5f) m_Pitch = 1.5f;
    if (m_Pitch < -1.5f) m_Pitch = -1.5f;

    Vec3 forward = {
        cosf(m_Pitch) * sinf(m_Yaw),
        sinf(m_Pitch),
        cosf(m_Pitch) * cosf(m_Yaw)
    };

    forward.normalized();
    Vec3 right = { sinf(m_Yaw - PI/2), 0, cosf(m_Yaw - PI/2) };
    if (IsKeyDown(KeyboardKey::W)) m_Cam.position = m_Cam.position + (forward * (m_Speed * dt));
    if (IsKeyDown(KeyboardKey::S)) m_Cam.position = m_Cam.position - (forward * (m_Speed * dt));
    if (IsKeyDown(KeyboardKey::A)) m_Cam.position = m_Cam.position - (right * (m_Speed * dt));
    if (IsKeyDown(KeyboardKey::D)) m_Cam.position = m_Cam.position + (right * (m_Speed * dt));

    m_Cam.target = m_Cam.position + forward;
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
