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

    static Vec2 GetMousePosition()
    {
        if (g_InputCallbacks.pfnGetMousePosition)
        {
            return g_InputCallbacks.pfnGetMousePosition();
        }
        return Vec2{ 0.0f, 0.0f };
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
using ScriptEntity = ::quark::script::CScriptEntity;

#define REGISTER_SCRIPT(ScriptClass) \
    extern "C" QUARK_SCRIPT_API IScript* Create_##ScriptClass() \
    { \
        return new ScriptClass(); \
    } \
    extern "C" QUARK_SCRIPT_API void Destroy_##ScriptClass(IScript* pScript) \
    { \
        delete pScript; \
    } \
    namespace { \
    struct SScriptAutoRegister_##ScriptClass \
    { \
        SScriptAutoRegister_##ScriptClass() \
        { \
            ::quark::script::CScriptRegistry::Get().Register( \
                #ScriptClass, &Create_##ScriptClass, &Destroy_##ScriptClass); \
        } \
    } s_ScriptAutoRegister_##ScriptClass; \
    }

#endif // __QUARK_ENGINE_SDK_H__
