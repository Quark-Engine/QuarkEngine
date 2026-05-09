#pragma once
#include "raylib.h"
#include "scene.h"

class FlyCamera {
public:
    Camera3D cam;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float speed = 2;
    float sensitivity = 0.003f;
    bool active = false;
    
    FlyCamera();
    void update(Scene& scene);
    Camera3D &get_camera();
};