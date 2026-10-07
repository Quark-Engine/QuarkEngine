#ifndef __CAMERA_H__
#define __CAMERA_H__
#include "QuarkCore/QuarkCore.hpp"
#include "editor/editor_preferences.h"
#include "scene.h"

class CFlyCamera
{
public:
    Camera3D m_Cam;
    float m_Pitch = 0.0f;
    float m_Yaw = 0.0f;
    float m_Speed = 2;
    float m_Sensitivity = 0.003f;
    float m_ZoomSensitivity = 1.0f;
    bool m_Active = false;
    
    CFlyCamera();
    void Update(CScene& scene, CPreferences& preferences);
    void FocusOn(const Vec3& point);
    Camera3D& GetCamera();
};

#endif // __CAMERA_H__
