#pragma once
#include "QuarkCore/QuarkCore.hpp"
#include "scene.h"

using namespace qc;

class FlyCamera {
public:
    Camera3D cam;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float speed = 2;
    float sensitivity = 0.003f;
    float zoom_sensitivity = 1.0f;
    bool active = false;
    
    FlyCamera();
    void update(Scene& scene);
    void focus_on(const Vec3& point);
    Camera3D &get_camera();
};
