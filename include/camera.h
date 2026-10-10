#ifndef __CAMERA_H__
#define __CAMERA_H__
#include "QuarkCore/QuarkCore.hpp"
#include "editor/editor_preferences.h"
#include "scene.h"

struct SDL_Cursor;

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
    ~CFlyCamera();
    void InitializeCursors();
    void Update(CScene& scene, CPreferences& preferences);
    void ApplyActiveCursor();
    void FocusOn(const Vec3& point);
    Camera3D& GetCamera();

private:
    enum class EMouseControl
    {
        NONE,
        ORBIT,
        PAN,
    };

    void UpdateOrbitCamera(float wheel);
    void SetImGuiCursorOverride(bool enabled);

    EMouseControl m_ActiveMouseControl = EMouseControl::NONE;
    float m_LastMouseX = 0.0f;
    float m_LastMouseY = 0.0f;
    bool m_ImGuiCursorOverride = false;
    bool m_PreviousImGuiCursorChangeDisabled = false;
    SDL_Cursor* m_pOrbitCursor = nullptr;
    SDL_Cursor* m_pPanCursor = nullptr;
};

#endif // __CAMERA_H__
