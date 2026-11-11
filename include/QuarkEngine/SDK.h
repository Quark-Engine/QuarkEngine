#ifndef __QUARK_ENGINE_SDK_H__
#define __QUARK_ENGINE_SDK_H__

#include "QuarkCore/QuarkCore.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#if defined(_WIN32)
#if defined(QUARK_SCRIPT_BUILD_DLL)
#define QUARK_SCRIPT_API __declspec(dllexport)
#else
#define QUARK_SCRIPT_API __declspec(dllexport)
#endif
#else
#define QUARK_SCRIPT_API __attribute__((visibility("default")))
#endif

class CEntity;
class CTransformComponent;
class CMeshComponent;
class CMaterialComponent;
class CLightComponent;
class CCameraComponent;
class CCollisionComponent;

namespace quark::script
{

    struct SScriptTransform
    {
        Vec3& position;
        Vec3& rotation;
        Vec3& scale;

        Vec3 GetForward() const
        {
            const Mat4 rotMat = Mat4::rotationY(rotation.y * DEG2RAD) *
                Mat4::rotationX(rotation.x * DEG2RAD) *
                Mat4::rotationZ(rotation.z * DEG2RAD);
            const Vec3 fwd = Vec3(rotMat * Vec3{ 0.0f, 0.0f, -1.0f });
            return fwd.normalized();
        }

        Vec3 GetRight() const
        {
            const Mat4 rotMat = Mat4::rotationY(rotation.y * DEG2RAD) *
                Mat4::rotationX(rotation.x * DEG2RAD) *
                Mat4::rotationZ(rotation.z * DEG2RAD);
            const Vec3 right = Vec3(rotMat * Vec3{ 1.0f, 0.0f, 0.0f });
            return right.normalized();
        }

        Vec3 GetUp() const
        {
            const Mat4 rotMat = Mat4::rotationY(rotation.y * DEG2RAD) *
                Mat4::rotationX(rotation.x * DEG2RAD) *
                Mat4::rotationZ(rotation.z * DEG2RAD);
            const Vec3 up = Vec3(rotMat * Vec3{ 0.0f, 1.0f, 0.0f });
            return up.normalized();
        }

        void LookAt(const Vec3& target)
        {
            const Vec3 dir = target - position;
            const float lenSq = dir.lengthSquared();
            if (lenSq < 0.000001f)
            {
                return;
            }

            const Vec3 normDir = dir * (1.0f / std::sqrt(lenSq));
            const float pitch = std::asin(std::clamp(normDir.y, -1.0f, 1.0f)) * RAD2DEG;
            const float yaw = std::atan2(-normDir.x, -normDir.z) * RAD2DEG;

            rotation.x = pitch;
            rotation.y = yaw;
            rotation.z = 0.0f;
        }
    };

    class CScriptEntity
    {
    public:
        explicit CScriptEntity(CEntity* pEntity = nullptr,
                               Vec3* pPos = nullptr,
                               Vec3* pRot = nullptr,
                               Vec3* pScale = nullptr,
                               bool* pHasOverride = nullptr)
            : m_pEntity(pEntity),
              m_pPosition(pPos),
              m_pRotation(pRot),
              m_pScale(pScale),
              m_pHasLocalMatrixOverride(pHasOverride)
        {
        }

        CEntity* GetRawEntity() const
        {
            return m_pEntity;
        }

        SScriptTransform GetTransform()
        {
            if (m_pHasLocalMatrixOverride)
            {
                *m_pHasLocalMatrixOverride = false;
            }
            if (m_pPosition && m_pRotation && m_pScale)
            {
                return SScriptTransform{ *m_pPosition, *m_pRotation, *m_pScale };
            }
            return GetFallbackTransform();
        }

        void SetTransformPointers(Vec3* pPos, Vec3* pRot, Vec3* pScale, bool* pHasOverride = nullptr)
        {
            m_pPosition = pPos;
            m_pRotation = pRot;
            m_pScale = pScale;
            m_pHasLocalMatrixOverride = pHasOverride;
        }

        Vec3 GetForward()
        {
            return GetTransform().GetForward();
        }

        Vec3 GetRight()
        {
            return GetTransform().GetRight();
        }

        Vec3 GetUp()
        {
            return GetTransform().GetUp();
        }

        void LookAt(const Vec3& target)
        {
            GetTransform().LookAt(target);
        }

        int GetId() const;
        std::string GetName() const;

        CTransformComponent* GetTransformComponent();
        CMeshComponent* GetMeshComponent();
        CMaterialComponent* GetMaterialComponent();
        CLightComponent* GetLightComponent();
        CCameraComponent* GetCameraComponent();
        CCollisionComponent* GetCollisionComponent();

    private:
        SScriptTransform GetFallbackTransform();

        CEntity* m_pEntity = nullptr;
        Vec3* m_pPosition = nullptr;
        Vec3* m_pRotation = nullptr;
        Vec3* m_pScale = nullptr;
        bool* m_pHasLocalMatrixOverride = nullptr;
    };

    class IScript
    {
    public:
        CScriptEntity* m_Entity = nullptr;

        virtual ~IScript() = default;

        virtual void OnCreate()
        {
        }

        virtual void OnStart()
        {
        }

        virtual void OnUpdate(float deltaTime)
        {
        }

        virtual void OnDestroy()
        {
        }

        virtual void SetPropertyFloat(const std::string& name, float value)
        {
        }

        virtual void SetPropertyInt(const std::string& name, int value)
        {
        }

        virtual void SetPropertyBool(const std::string& name, bool value)
        {
        }

        virtual void SetPropertyString(const std::string& name, const std::string& value)
        {
        }

        virtual void SetPropertyVec3(const std::string& name, const Vec3& value)
        {
        }

        virtual bool GetPropertyFloat(const std::string& name, float& outValue) const
        {
            return false;
        }

        virtual bool GetPropertyInt(const std::string& name, int& outValue) const
        {
            return false;
        }

        virtual bool GetPropertyBool(const std::string& name, bool& outValue) const
        {
            return false;
        }

        virtual bool GetPropertyString(const std::string& name, std::string& outValue) const
        {
            return false;
        }

        virtual bool GetPropertyVec3(const std::string& name, Vec3& outValue) const
        {
            return false;
        }
    };

    enum class Key
    {
        Null = KEY_NULL,
        Apostrophe = KEY_APOSTROPHE,
        Comma = KEY_COMMA,
        Minus = KEY_MINUS,
        Period = KEY_PERIOD,
        Slash = KEY_SLASH,
        Zero = KEY_ZERO,
        One = KEY_ONE,
        Two = KEY_TWO,
        Three = KEY_THREE,
        Four = KEY_FOUR,
        Five = KEY_FIVE,
        Six = KEY_SIX,
        Seven = KEY_SEVEN,
        Eight = KEY_EIGHT,
        Nine = KEY_NINE,
        Semicolon = KEY_SEMICOLON,
        Equal = KEY_EQUAL,
        A = KEY_A,
        B = KEY_B,
        C = KEY_C,
        D = KEY_D,
        E = KEY_E,
        F = KEY_F,
        G = KEY_G,
        H = KEY_H,
        I = KEY_I,
        J = KEY_J,
        K = KEY_K,
        L = KEY_L,
        M = KEY_M,
        N = KEY_N,
        O = KEY_O,
        P = KEY_P,
        Q = KEY_Q,
        R = KEY_R,
        S = KEY_S,
        T = KEY_T,
        U = KEY_U,
        V = KEY_V,
        W = KEY_W,
        X = KEY_X,
        Y = KEY_Y,
        Z = KEY_Z,
        Space = KEY_SPACE,
        Escape = KEY_ESCAPE,
        Enter = KEY_ENTER,
        Tab = KEY_TAB,
        Backspace = KEY_BACKSPACE,
        Insert = KEY_INSERT,
        Delete = KEY_DELETE,
        Right = KEY_RIGHT,
        Left = KEY_LEFT,
        Down = KEY_DOWN,
        Up = KEY_UP,
        PageUp = KEY_PAGE_UP,
        PageDown = KEY_PAGE_DOWN,
        Home = KEY_HOME,
        End = KEY_END,
        CapsLock = KEY_CAPS_LOCK,
        ScrollLock = KEY_SCROLL_LOCK,
        NumLock = KEY_NUM_LOCK,
        PrintScreen = KEY_PRINT_SCREEN,
        Pause = KEY_PAUSE,
        F1 = KEY_F1,
        F2 = KEY_F2,
        F3 = KEY_F3,
        F4 = KEY_F4,
        F5 = KEY_F5,
        F6 = KEY_F6,
        F7 = KEY_F7,
        F8 = KEY_F8,
        F9 = KEY_F9,
        F10 = KEY_F10,
        F11 = KEY_F11,
        F12 = KEY_F12,
        LeftShift = KEY_LEFT_SHIFT,
        LeftControl = KEY_LEFT_CONTROL,
        LeftAlt = KEY_LEFT_ALT,
        LeftSuper = KEY_LEFT_SUPER,
        RightShift = KEY_RIGHT_SHIFT,
        RightControl = KEY_RIGHT_CONTROL,
        RightAlt = KEY_RIGHT_ALT,
        RightSuper = KEY_RIGHT_SUPER
    };

    enum class Mouse
    {
        Left = MOUSE_BUTTON_LEFT,
        Right = MOUSE_BUTTON_RIGHT,
        Middle = MOUSE_BUTTON_MIDDLE,
        Side = MOUSE_BUTTON_SIDE,
        Extra = MOUSE_BUTTON_EXTRA,
        Forward = MOUSE_BUTTON_FORWARD,
        Back = MOUSE_BUTTON_BACK
    };

    struct SInputCallbacks
    {
        bool (*pfnIsKeyDown)(int key) = nullptr;
        bool (*pfnIsKeyPressed)(int key) = nullptr;
        bool (*pfnIsKeyReleased)(int key) = nullptr;
        bool (*pfnIsKeyUp)(int key) = nullptr;
        bool (*pfnIsMouseButtonDown)(int button) = nullptr;
        bool (*pfnIsMouseButtonPressed)(int button) = nullptr;
        bool (*pfnIsMouseButtonReleased)(int button) = nullptr;
        bool (*pfnIsMouseButtonUp)(int button) = nullptr;
        Vec2 (*pfnGetMousePosition)() = nullptr;
        Vec2 (*pfnGetMouseDelta)() = nullptr;
        float (*pfnGetMouseWheelMove)() = nullptr;
        void (*pfnSetCursorLocked)(bool locked) = nullptr;
        bool (*pfnIsCursorLocked)() = nullptr;
        void (*pfnSetCursorVisible)(bool visible) = nullptr;
        bool (*pfnIsCursorVisible)() = nullptr;
        bool (*pfnIsViewportHovered)() = nullptr;
    };

    extern SInputCallbacks g_InputCallbacks;

    class Input
    {
    public:
        static bool IsKeyDown(Key key)
        {
            if (g_InputCallbacks.pfnIsKeyDown)
            {
                return g_InputCallbacks.pfnIsKeyDown(static_cast<int>(key));
            }
            return false;
        }

        static bool IsKeyPressed(Key key)
        {
            if (g_InputCallbacks.pfnIsKeyPressed)
            {
                return g_InputCallbacks.pfnIsKeyPressed(static_cast<int>(key));
            }
            return false;
        }

        static bool IsKeyReleased(Key key)
        {
            if (g_InputCallbacks.pfnIsKeyReleased)
            {
                return g_InputCallbacks.pfnIsKeyReleased(static_cast<int>(key));
            }
            return false;
        }

        static bool IsKeyUp(Key key)
        {
            if (g_InputCallbacks.pfnIsKeyUp)
            {
                return g_InputCallbacks.pfnIsKeyUp(static_cast<int>(key));
            }
            return false;
        }

        static bool IsMouseButtonDown(int button)
        {
            if (g_InputCallbacks.pfnIsMouseButtonDown)
            {
                return g_InputCallbacks.pfnIsMouseButtonDown(button);
            }
            return false;
        }

        static bool IsMouseButtonDown(MouseButton button)
        {
            return IsMouseButtonDown(static_cast<int>(button));
        }

        static bool IsMouseButtonDown(Mouse button)
        {
            return IsMouseButtonDown(static_cast<int>(button));
        }

        static bool IsMouseButtonPressed(int button)
        {
            if (g_InputCallbacks.pfnIsMouseButtonPressed)
            {
                return g_InputCallbacks.pfnIsMouseButtonPressed(button);
            }
            return false;
        }

        static bool IsMouseButtonPressed(MouseButton button)
        {
            return IsMouseButtonPressed(static_cast<int>(button));
        }

        static bool IsMouseButtonPressed(Mouse button)
        {
            return IsMouseButtonPressed(static_cast<int>(button));
        }

        static bool IsMouseButtonReleased(int button)
        {
            if (g_InputCallbacks.pfnIsMouseButtonReleased)
            {
                return g_InputCallbacks.pfnIsMouseButtonReleased(button);
            }
            return false;
        }

        static bool IsMouseButtonReleased(MouseButton button)
        {
            return IsMouseButtonReleased(static_cast<int>(button));
        }

        static bool IsMouseButtonReleased(Mouse button)
        {
            return IsMouseButtonReleased(static_cast<int>(button));
        }

        static bool IsMouseButtonUp(int button)
        {
            if (g_InputCallbacks.pfnIsMouseButtonUp)
            {
                return g_InputCallbacks.pfnIsMouseButtonUp(button);
            }
            return false;
        }

        static bool IsMouseButtonUp(MouseButton button)
        {
            return IsMouseButtonUp(static_cast<int>(button));
        }

        static bool IsMouseButtonUp(Mouse button)
        {
            return IsMouseButtonUp(static_cast<int>(button));
        }

        static Vec2 GetMousePosition()
        {
            if (g_InputCallbacks.pfnGetMousePosition)
            {
                return g_InputCallbacks.pfnGetMousePosition();
            }
            return Vec2{ 0.0f, 0.0f };
        }

        static Vec2 GetMouseDelta()
        {
            if (g_InputCallbacks.pfnGetMouseDelta)
            {
                return g_InputCallbacks.pfnGetMouseDelta();
            }
            return Vec2{ 0.0f, 0.0f };
        }

        static float GetMouseWheelMove()
        {
            if (g_InputCallbacks.pfnGetMouseWheelMove)
            {
                return g_InputCallbacks.pfnGetMouseWheelMove();
            }
            return 0.0f;
        }

        static void SetCursorLocked(bool locked)
        {
            if (g_InputCallbacks.pfnSetCursorLocked)
            {
                g_InputCallbacks.pfnSetCursorLocked(locked);
            }
        }

        static bool IsCursorLocked()
        {
            if (g_InputCallbacks.pfnIsCursorLocked)
            {
                return g_InputCallbacks.pfnIsCursorLocked();
            }
            return false;
        }

        static void SetCursorVisible(bool visible)
        {
            if (g_InputCallbacks.pfnSetCursorVisible)
            {
                g_InputCallbacks.pfnSetCursorVisible(visible);
            }
        }

        static bool IsCursorVisible()
        {
            if (g_InputCallbacks.pfnIsCursorVisible)
            {
                return g_InputCallbacks.pfnIsCursorVisible();
            }
            return true;
        }

        static bool IsViewportHovered()
        {
            if (g_InputCallbacks.pfnIsViewportHovered)
            {
                return g_InputCallbacks.pfnIsViewportHovered();
            }
            return true;
        }
    };

    struct STimeCallbacks
    {
        float (*pfnGetDeltaTime)() = nullptr;
        float (*pfnGetUnscaledDeltaTime)() = nullptr;
        double (*pfnGetTime)() = nullptr;
        float (*pfnGetTimeScale)() = nullptr;
        void (*pfnSetTimeScale)(float scale) = nullptr;
    };

    extern STimeCallbacks g_TimeCallbacks;

    class Time
    {
    public:
        static float GetDeltaTime()
        {
            if (g_TimeCallbacks.pfnGetDeltaTime)
            {
                return g_TimeCallbacks.pfnGetDeltaTime();
            }
            return 0.0166667f;
        }

        static float GetUnscaledDeltaTime()
        {
            if (g_TimeCallbacks.pfnGetUnscaledDeltaTime)
            {
                return g_TimeCallbacks.pfnGetUnscaledDeltaTime();
            }
            return 0.0166667f;
        }

        static double GetTime()
        {
            if (g_TimeCallbacks.pfnGetTime)
            {
                return g_TimeCallbacks.pfnGetTime();
            }
            return 0.0;
        }

        static float GetTimeScale()
        {
            if (g_TimeCallbacks.pfnGetTimeScale)
            {
                return g_TimeCallbacks.pfnGetTimeScale();
            }
            return 1.0f;
        }

        static void SetTimeScale(float scale)
        {
            if (g_TimeCallbacks.pfnSetTimeScale)
            {
                g_TimeCallbacks.pfnSetTimeScale(scale);
            }
        }
    };

    class Math
    {
    public:
        static constexpr float Pi = 3.14159265358979323846f;
        static constexpr float Deg2Rad = Pi / 180.0f;
        static constexpr float Rad2Deg = 180.0f / Pi;

        static float Clamp(float value, float min, float max)
        {
            return value < min ? min : (value > max ? max : value);
        }

        static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
        {
            return a + (b - a) * t;
        }

        static float Distance(const Vec3& a, const Vec3& b)
        {
            return (b - a).length();
        }

        static float DistanceSquared(const Vec3& a, const Vec3& b)
        {
            return (b - a).lengthSquared();
        }

        static float Dot(const Vec3& a, const Vec3& b)
        {
            return a.dot(b);
        }

        static Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return a.cross(b);
        }

        static Vec3 Normalize(const Vec3& v)
        {
            return v.normalized();
        }
    };

    using FScriptCreator = IScript* (*)();
    using FScriptDestroyer = void (*)(IScript*);

    class CScriptRegistry
    {
    public:
        static CScriptRegistry& Get()
        {
            static CScriptRegistry s_Instance;
            return s_Instance;
        }

        void Register(const std::string& name, FScriptCreator pfnCreator, FScriptDestroyer pfnDestroyer)
        {
            m_Creators[name] = pfnCreator;
            m_Destroyers[name] = pfnDestroyer;
        }

        IScript* Create(const std::string& name) const
        {
            const auto it = m_Creators.find(name);
            if (it != m_Creators.end() && it->second)
            {
                return it->second();
            }
            return nullptr;
        }

        void Destroy(const std::string& name, IScript* pScript) const
        {
            const auto it = m_Destroyers.find(name);
            if (it != m_Destroyers.end() && it->second)
            {
                it->second(pScript);
            }
            else
            {
                delete pScript;
            }
        }

        std::vector<std::string> GetRegisteredNames() const
        {
            std::vector<std::string> vNames;
            vNames.reserve(m_Creators.size());
            for (const auto& pair : m_Creators)
            {
                vNames.push_back(pair.first);
            }
            return vNames;
        }

    private:
        std::unordered_map<std::string, FScriptCreator> m_Creators;
        std::unordered_map<std::string, FScriptDestroyer> m_Destroyers;
    };

} // quark::script

using IScript = ::quark::script::IScript;
using Input = ::quark::script::Input;
using Key = ::quark::script::Key;
using Mouse = ::quark::script::Mouse;
using MouseBtn = ::quark::script::Mouse;
using Time = ::quark::script::Time;
using Math = ::quark::script::Math;
using ScriptEntity = ::quark::script::CScriptEntity;

#define REGISTER_SCRIPT(ScriptClass)                                              \
    extern "C" QUARK_SCRIPT_API IScript* Create_##ScriptClass()                   \
    {                                                                             \
        return new ScriptClass();                                                 \
    }                                                                             \
    extern "C" QUARK_SCRIPT_API void Destroy_##ScriptClass(IScript* pScript)      \
    {                                                                             \
        delete pScript;                                                           \
    }                                                                             \
    namespace                                                                     \
    {                                                                             \
        struct SScriptAutoRegister_##ScriptClass                                  \
        {                                                                         \
            SScriptAutoRegister_##ScriptClass()                                   \
            {                                                                     \
                ::quark::script::CScriptRegistry::Get().Register(                 \
                    #ScriptClass, &Create_##ScriptClass, &Destroy_##ScriptClass); \
            }                                                                     \
        } s_ScriptAutoRegister_##ScriptClass;                                     \
    }

#endif // __QUARK_ENGINE_SDK_H__
