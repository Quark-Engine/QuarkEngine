#include "headers/camera.h"
#include <cmath>
#include "raymath.h"
#include "imgui.h"
#include "headers/ImGuizmo.h"

FlyCamera::FlyCamera() {
    cam.position = {5.0f, 5.0f, 5.0f};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Vector3 dir = Vector3Subtract(cam.target, cam.position);
    yaw = atan2f(dir.x, dir.z);
    pitch = asinf(dir.y / sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z));
}

void FlyCamera::update(Scene& scene) {
    if (!active && ImGuizmo::IsUsing())
        return;

    Entity* selected = scene.get_selected();
    MeshComponent* sel_mesh = selected ? selected->get_mesh_component() : nullptr;
    if (sel_mesh && sel_mesh->vertex_gizmo) return;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ImGuizmo::IsOver()) {
        DisableCursor();
        active = true;
        
        Vector2 winCenter = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
        SetMousePosition(winCenter.x, winCenter.y);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        EnableCursor();
        active = false;
    }

    if (!active) return;

    float dt = GetFrameTime();
    Vector2 md = GetMouseDelta();
    if (fabs(md.x) > 100 || fabs(md.y) > 100) md = {0,0};

    yaw   -= md.x * sensitivity;
    pitch -= md.y * sensitivity;

    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;

    Vector3 forward = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };

    forward = Vector3Normalize(forward);
    Vector3 right = { sinf(yaw - PI/2), 0, cosf(yaw - PI/2) };

    if (IsKeyDown(KEY_W)) cam.position = Vector3Add(cam.position, Vector3Scale(forward, speed * dt));
    if (IsKeyDown(KEY_S)) cam.position = Vector3Subtract(cam.position, Vector3Scale(forward, speed * dt));
    if (IsKeyDown(KEY_A)) cam.position = Vector3Subtract(cam.position, Vector3Scale(right, speed * dt));
    if (IsKeyDown(KEY_D)) cam.position = Vector3Add(cam.position, Vector3Scale(right, speed * dt));

    cam.target = Vector3Add(cam.position, forward);
    Vector2 winCenter = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    SetMousePosition(winCenter.x, winCenter.y);
}

Camera3D& FlyCamera::get_camera() {
    return cam;
}