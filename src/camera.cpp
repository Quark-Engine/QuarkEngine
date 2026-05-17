#include "headers/camera.h"
#include <cmath>
#include "imgui.h"
#include "headers/ImGuizmo.h"

FlyCamera::FlyCamera() {
    cam.position = {5.0f, 5.0f, 5.0f};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Vec3 dir = cam.target - cam.position;
    yaw = atan2f(dir.x, dir.z);
    pitch = asinf(dir.y / sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z));
}

void FlyCamera::update(Scene& scene) {
    if (!active && ImGuizmo::IsUsing())
        return;

    Entity* selected = scene.get_selected();
    MeshComponent* sel_mesh = selected ? selected->get_mesh_component() : nullptr;
    if (sel_mesh && sel_mesh->vertex_gizmo) return;

    if (IsMouseButtonPressed(MouseButton::Left) && !ImGuizmo::IsOver()) {
        DisableCursor();
        active = true;
        
        Vec2 winCenter = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
        SetMousePosition(winCenter.x, winCenter.y);
    }

    if (IsKeyPressed(KeyboardKey::Escape)) {
        EnableCursor();
        active = false;
    }

    if (!active) return;

    float dt = GetDeltaTime();
    Vec2 md = GetMouseDelta();
    if (fabs(md.x) > 100 || fabs(md.y) > 100) md = {0,0};

    yaw   -= md.x * sensitivity;
    pitch -= md.y * sensitivity;

    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;

    Vec3 forward = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };

    forward.normalized();
    Vec3 right = { sinf(yaw - PI/2), 0, cosf(yaw - PI/2) };
    if (IsKeyDown(KeyboardKey::W)) cam.position = cam.position + (forward * (speed * dt));
    if (IsKeyDown(KeyboardKey::S)) cam.position = cam.position - (forward * (speed * dt));
    if (IsKeyDown(KeyboardKey::A)) cam.position = cam.position - (right * (speed * dt));
    if (IsKeyDown(KeyboardKey::D)) cam.position = cam.position + (right * (speed * dt));

    cam.target = cam.position + forward;
    Vec2 winCenter = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    SetMousePosition(winCenter.x, winCenter.y);
}

Camera3D& FlyCamera::get_camera() {
    return cam;
}