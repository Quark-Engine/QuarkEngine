#ifndef __QUARK_ENGINE_SDK_H__
#define __QUARK_ENGINE_SDK_H__

#include "QuarkCore/QuarkCore.hpp"
#include <cstdint>
#include <string>
#include <type_traits>
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
class CRigidbodyComponent;

namespace quark::script
{

    class CScriptEntity;
    class IScript;

    struct SRay
    {
        Vec3 m_Origin = { 0.0f, 0.0f, 0.0f };
        Vec3 m_Direction = { 0.0f, 0.0f, -1.0f };
    };

    struct SEntityRef
    {
        int m_EntityId = -1;
        CScriptEntity* m_pResolved = nullptr;

        SEntityRef() = default;
        explicit SEntityRef(int entityId)
            : m_EntityId(entityId)
        {
        }

        bool IsValid() const
        {
            return m_EntityId >= 0;
        }
    };

    struct SAssetRef
    {
        std::string m_Path;

        SAssetRef() = default;
        explicit SAssetRef(const std::string& path)
            : m_Path(path)
        {
        }

        bool IsEmpty() const
        {
            return m_Path.empty();
        }
    };

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

        void SetRawEntity(CEntity* pEntity)
        {
            m_pEntity = pEntity;
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
        void SetName(const std::string& name);
        bool HasTag(const std::string& tag) const;
        void AddTag(const std::string& tag);

        Vec3 GetWorldPosition() const;
        void SetParent(CScriptEntity* pParent, bool keepWorldTransform = true);
        CScriptEntity* GetParent() const;
        std::vector<CScriptEntity*> GetChildren() const;
        void SetActive(bool active);
        bool IsActive() const;

        IScript* GetScriptRaw() const;
        template <typename T>
        T* GetScript() const
        {
            return dynamic_cast<T*>(GetScriptRaw());
        }

        Color GetMaterialColor() const;
        void SetMaterialColor(const Color& color);

        Color GetLightColor() const;
        void SetLightColor(const Color& color);
        float GetLightIntensity() const;
        void SetLightIntensity(float intensity);
        float GetLightRange() const;
        void SetLightRange(float range);
        bool IsLightEnabled() const;
        void SetLightEnabled(bool enabled);

        CTransformComponent* GetTransformComponent();
        CMeshComponent* GetMeshComponent();
        CMaterialComponent* GetMaterialComponent();
        CLightComponent* GetLightComponent();
        CCameraComponent* GetCameraComponent();
        CCollisionComponent* GetCollisionComponent();
        CRigidbodyComponent* GetRigidbodyComponent();

    private:
        SScriptTransform GetFallbackTransform();

        CEntity* m_pEntity = nullptr;
        Vec3* m_pPosition = nullptr;
        Vec3* m_pRotation = nullptr;
        Vec3* m_pScale = nullptr;
        bool* m_pHasLocalMatrixOverride = nullptr;
    };

    struct SCollisionInfo
    {
        CScriptEntity* m_pOther = nullptr;
        Vec3 m_Point = { 0.0f, 0.0f, 0.0f };
        Vec3 m_Normal = { 0.0f, 1.0f, 0.0f };
        float m_Penetration = 0.0f;
    };

    struct SRaycastHit
    {
        bool m_Hit = false;
        CScriptEntity* m_pEntity = nullptr;
        int m_EntityId = -1;
        Vec3 m_Point = { 0.0f, 0.0f, 0.0f };
        Vec3 m_Normal = { 0.0f, 1.0f, 0.0f };
        float m_Distance = 0.0f;
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

        virtual void OnFixedUpdate(float fixedDeltaTime)
        {
        }

        virtual void OnLateUpdate(float deltaTime)
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

        virtual void SetPropertyColor(const std::string& name, const Color& value)
        {
        }

        virtual void SetPropertyEntityRef(const std::string& name, const SEntityRef& value)
        {
        }

        virtual void SetPropertyAssetRef(const std::string& name, const SAssetRef& value)
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

        virtual bool GetPropertyColor(const std::string& name, Color& outValue) const
        {
            return false;
        }

        virtual bool GetPropertyEntityRef(const std::string& name, SEntityRef& outValue) const
        {
            return false;
        }

        virtual bool GetPropertyAssetRef(const std::string& name, SAssetRef& outValue) const
        {
            return false;
        }

        virtual void OnUIEvent(const std::string& eventName, const std::string& actionName)
        {
        }

        virtual void OnCollisionEnter(const SCollisionInfo& collision)
        {
        }

        virtual void OnCollisionStay(const SCollisionInfo& collision)
        {
        }

        virtual void OnCollisionExit(CScriptEntity* pOther)
        {
        }

        virtual void OnTriggerEnter(CScriptEntity* pOther)
        {
        }

        virtual void OnTriggerStay(CScriptEntity* pOther)
        {
        }

        virtual void OnTriggerExit(CScriptEntity* pOther)
        {
        }

        std::uint64_t Invoke(float delaySeconds, std::function<void()> callback);
        std::uint64_t InvokeRepeating(float delaySeconds, float repeatRateSeconds, std::function<void()> callback);
        void CancelInvoke(std::uint64_t timerId);
        void CancelAllInvokes();
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

        static float GetAxis(const std::string& axisName)
        {
            if (axisName == "Horizontal")
            {
                float value = 0.0f;
                if (IsKeyDown(Key::D) || IsKeyDown(Key::Right))
                {
                    value += 1.0f;
                }
                if (IsKeyDown(Key::A) || IsKeyDown(Key::Left))
                {
                    value -= 1.0f;
                }
                return value;
            }
            if (axisName == "Vertical")
            {
                float value = 0.0f;
                if (IsKeyDown(Key::W) || IsKeyDown(Key::Up))
                {
                    value += 1.0f;
                }
                if (IsKeyDown(Key::S) || IsKeyDown(Key::Down))
                {
                    value -= 1.0f;
                }
                return value;
            }
            if (axisName == "Mouse X")
            {
                return GetMouseDelta().x;
            }
            if (axisName == "Mouse Y")
            {
                return GetMouseDelta().y;
            }
            if (axisName == "Mouse ScrollWheel")
            {
                return GetMouseWheelMove();
            }
            return 0.0f;
        }
    };

    struct STimeCallbacks
    {
        float (*pfnGetDeltaTime)() = nullptr;
        float (*pfnGetUnscaledDeltaTime)() = nullptr;
        float (*pfnGetFixedDeltaTime)() = nullptr;
        void (*pfnSetFixedDeltaTime)(float fixedStep) = nullptr;
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

        static float GetFixedDeltaTime()
        {
            if (g_TimeCallbacks.pfnGetFixedDeltaTime)
            {
                return g_TimeCallbacks.pfnGetFixedDeltaTime();
            }
            return 0.02f;
        }

        static void SetFixedDeltaTime(float fixedStep)
        {
            if (g_TimeCallbacks.pfnSetFixedDeltaTime)
            {
                g_TimeCallbacks.pfnSetFixedDeltaTime(fixedStep);
            }
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

    enum class EScriptPrimitiveType
    {
        CUBE = 0,
        SPHERE = 1,
        CONE = 2,
        CYLINDER = 3,
        TORUS = 4,
        HEMISPHERE = 5,
        CAPSULE = 6
    };

    enum class EScriptLightType
    {
        DIRECTIONAL = 0,
        POINT = 1,
        SPOT = 2,
        AREA = 3
    };

    struct SSceneCallbacks
    {
        CScriptEntity* (*pfnFindEntityByName)(const char* pName) = nullptr;
        CScriptEntity* (*pfnFindEntityById)(int entityId) = nullptr;
        int (*pfnFindEntitiesWithTag)(const char* pTag, CScriptEntity** ppOutEntities, int maxCount) = nullptr;
        CScriptEntity* (*pfnInstantiateEntity)(CScriptEntity* pSourceEntity) = nullptr;
        CScriptEntity* (*pfnInstantiatePrefab)(const char* pPrefabPath) = nullptr;
        CScriptEntity* (*pfnCreateEntity)(const char* pName, Vec3 position) = nullptr;
        CScriptEntity* (*pfnCreatePrimitive)(int primitiveType, const char* pName, Vec3 position, Vec3 scale) = nullptr;
        void (*pfnDestroyEntity)(CScriptEntity* pEntity) = nullptr;
        void (*pfnDestroyEntityById)(int entityId) = nullptr;
        bool (*pfnLoadScene)(const char* pScenePath) = nullptr;
        bool (*pfnReloadScene)() = nullptr;
        const char* (*pfnGetActiveSceneName)() = nullptr;
        const char* (*pfnGetActiveScenePath)() = nullptr;
        int (*pfnGetSceneCount)() = nullptr;
        const char* (*pfnGetScenePath)(int index) = nullptr;
        void (*pfnDontDestroyOnLoad)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetPersistentString)(const char* pKey, const char* pValue) = nullptr;
        bool (*pfnGetPersistentString)(const char* pKey, char* pOutBuffer, int bufferSize) = nullptr;
        void (*pfnSetPersistentInt)(const char* pKey, int value) = nullptr;
        int (*pfnGetPersistentInt)(const char* pKey, int defaultValue) = nullptr;
        void (*pfnSetPersistentFloat)(const char* pKey, float value) = nullptr;
        float (*pfnGetPersistentFloat)(const char* pKey, float defaultValue) = nullptr;
        void (*pfnSetPersistentBool)(const char* pKey, bool value) = nullptr;
        bool (*pfnGetPersistentBool)(const char* pKey, bool defaultValue) = nullptr;
        bool (*pfnHasPersistentKey)(const char* pKey) = nullptr;
        void (*pfnRemovePersistentKey)(const char* pKey) = nullptr;
        void (*pfnClearPersistentData)() = nullptr;
        IScript* (*pfnGetEntityScript)(CScriptEntity* pEntity) = nullptr;
        Vec3 (*pfnGetWorldPosition)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetParent)(CScriptEntity* pEntity, CScriptEntity* pParent, bool keepWorldTransform) = nullptr;
        CScriptEntity* (*pfnGetParent)(CScriptEntity* pEntity) = nullptr;
        int (*pfnGetChildren)(CScriptEntity* pEntity, CScriptEntity** ppOutChildren, int maxCount) = nullptr;
        void (*pfnSetEntityActive)(CScriptEntity* pEntity, bool active) = nullptr;
        bool (*pfnIsEntityActive)(CScriptEntity* pEntity) = nullptr;
        std::uint64_t (*pfnInvokeTimer)(IScript* pOwner, float delay, float repeatInterval,
                                        void (*pfnInvoke)(void*), void (*pfnDestroy)(void*),
                                        void* pUserData) = nullptr;
        void (*pfnCancelTimer)(std::uint64_t timerId) = nullptr;
        void (*pfnCancelOwnerTimers)(IScript* pOwner) = nullptr;
        std::uint64_t (*pfnSubscribeEvent)(const char* pEventName,
                                           void (*pfnInvoke)(const char*, void*),
                                           void (*pfnDestroy)(void*),
                                           void* pUserData) = nullptr;
        void (*pfnUnsubscribeEvent)(std::uint64_t subscriptionId) = nullptr;
        void (*pfnEmitEvent)(const char* pEventName, const char* pPayload) = nullptr;
        void (*pfnClearEvents)() = nullptr;
        CScriptEntity* (*pfnGetMainCamera)() = nullptr;
        SRay (*pfnScreenPointToRay)(Vec2 screenPos) = nullptr;
        float (*pfnGetCameraFov)(CScriptEntity* pCamera) = nullptr;
        void (*pfnSetCameraFov)(CScriptEntity* pCamera, float fov) = nullptr;
        void (*pfnApplicationQuit)() = nullptr;
        void (*pfnUiSetText)(CScriptEntity* pEntity, const char* pText) = nullptr;
        bool (*pfnUiGetText)(CScriptEntity* pEntity, char* pOutBuffer, int bufferSize) = nullptr;
        void (*pfnUiSetProgress)(CScriptEntity* pEntity, float value) = nullptr;
        float (*pfnUiGetProgress)(CScriptEntity* pEntity) = nullptr;
        void (*pfnUiSetSliderValue)(CScriptEntity* pEntity, float value) = nullptr;
        float (*pfnUiGetSliderValue)(CScriptEntity* pEntity) = nullptr;
        void (*pfnUiSetToggleValue)(CScriptEntity* pEntity, bool checked) = nullptr;
        bool (*pfnUiGetToggleValue)(CScriptEntity* pEntity) = nullptr;
        void (*pfnUiSetVisible)(CScriptEntity* pEntity, bool visible) = nullptr;
        bool (*pfnUiIsVisible)(CScriptEntity* pEntity) = nullptr;
        void (*pfnUiSetColor)(CScriptEntity* pEntity, Color color) = nullptr;
    };

    extern SSceneCallbacks g_SceneCallbacks;

    class Scene
    {
    public:
        static CScriptEntity* FindEntityByName(const std::string& name)
        {
            if (g_SceneCallbacks.pfnFindEntityByName)
            {
                return g_SceneCallbacks.pfnFindEntityByName(name.c_str());
            }
            return nullptr;
        }

        static CScriptEntity* FindEntityById(int entityId)
        {
            if (g_SceneCallbacks.pfnFindEntityById)
            {
                return g_SceneCallbacks.pfnFindEntityById(entityId);
            }
            return nullptr;
        }

        static std::vector<CScriptEntity*> FindEntitiesWithTag(const std::string& tag)
        {
            std::vector<CScriptEntity*> vResults;
            if (!g_SceneCallbacks.pfnFindEntitiesWithTag)
            {
                return vResults;
            }

            const int count = g_SceneCallbacks.pfnFindEntitiesWithTag(tag.c_str(), nullptr, 0);
            if (count <= 0)
            {
                return vResults;
            }

            vResults.resize(static_cast<size_t>(count), nullptr);
            const int written = g_SceneCallbacks.pfnFindEntitiesWithTag(
                tag.c_str(), vResults.data(), count);
            if (written < count)
            {
                vResults.resize(static_cast<size_t>(std::max(0, written)));
            }
            return vResults;
        }

        static CScriptEntity* CreateEntity(const std::string& name = "Entity",
                                           const Vec3& position = { 0.0f, 0.0f, 0.0f })
        {
            if (g_SceneCallbacks.pfnCreateEntity)
            {
                return g_SceneCallbacks.pfnCreateEntity(name.c_str(), position);
            }
            return nullptr;
        }

        static CScriptEntity* CreatePrimitive(EScriptPrimitiveType type,
                                              const std::string& name = "",
                                              const Vec3& position = { 0.0f, 0.0f, 0.0f },
                                              const Vec3& scale = { 1.0f, 1.0f, 1.0f })
        {
            if (g_SceneCallbacks.pfnCreatePrimitive)
            {
                return g_SceneCallbacks.pfnCreatePrimitive(static_cast<int>(type), name.c_str(), position, scale);
            }
            return nullptr;
        }

        static CScriptEntity* Instantiate(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnInstantiateEntity && pEntity)
            {
                return g_SceneCallbacks.pfnInstantiateEntity(pEntity);
            }
            return nullptr;
        }

        static CScriptEntity* Instantiate(CScriptEntity* pEntity,
                                          const Vec3& position,
                                          const Vec3& rotation = { 0.0f, 0.0f, 0.0f })
        {
            CScriptEntity* pSpawned = Instantiate(pEntity);
            if (pSpawned)
            {
                SScriptTransform tr = pSpawned->GetTransform();
                tr.position = position;
                tr.rotation = rotation;
            }
            return pSpawned;
        }

        static CScriptEntity* Instantiate(const std::string& prefabPath)
        {
            if (g_SceneCallbacks.pfnInstantiatePrefab && !prefabPath.empty())
            {
                return g_SceneCallbacks.pfnInstantiatePrefab(prefabPath.c_str());
            }
            return nullptr;
        }

        static CScriptEntity* Instantiate(const std::string& prefabPath,
                                          const Vec3& position,
                                          const Vec3& rotation = { 0.0f, 0.0f, 0.0f })
        {
            CScriptEntity* pSpawned = Instantiate(prefabPath);
            if (pSpawned)
            {
                SScriptTransform tr = pSpawned->GetTransform();
                tr.position = position;
                tr.rotation = rotation;
            }
            return pSpawned;
        }

        static CScriptEntity* Instantiate(const SAssetRef& prefabAsset)
        {
            return Instantiate(prefabAsset.m_Path);
        }

        static CScriptEntity* Instantiate(const SAssetRef& prefabAsset,
                                          const Vec3& position,
                                          const Vec3& rotation = { 0.0f, 0.0f, 0.0f })
        {
            return Instantiate(prefabAsset.m_Path, position, rotation);
        }

        static void Destroy(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnDestroyEntity && pEntity)
            {
                g_SceneCallbacks.pfnDestroyEntity(pEntity);
            }
        }

        static void Destroy(int entityId)
        {
            if (g_SceneCallbacks.pfnDestroyEntityById && entityId >= 0)
            {
                g_SceneCallbacks.pfnDestroyEntityById(entityId);
            }
        }

        static bool LoadScene(const std::string& scenePath)
        {
            if (g_SceneCallbacks.pfnLoadScene && !scenePath.empty())
            {
                return g_SceneCallbacks.pfnLoadScene(scenePath.c_str());
            }
            return false;
        }
    };

    class SceneManager
    {
    public:
        static bool LoadScene(const std::string& scenePath)
        {
            if (g_SceneCallbacks.pfnLoadScene && !scenePath.empty())
            {
                return g_SceneCallbacks.pfnLoadScene(scenePath.c_str());
            }
            return false;
        }

        static bool ReloadScene()
        {
            if (g_SceneCallbacks.pfnReloadScene)
            {
                return g_SceneCallbacks.pfnReloadScene();
            }
            return false;
        }

        static std::string GetActiveSceneName()
        {
            if (g_SceneCallbacks.pfnGetActiveSceneName)
            {
                const char* pName = g_SceneCallbacks.pfnGetActiveSceneName();
                return pName ? std::string(pName) : std::string();
            }
            return {};
        }

        static std::string GetActiveScenePath()
        {
            if (g_SceneCallbacks.pfnGetActiveScenePath)
            {
                const char* pPath = g_SceneCallbacks.pfnGetActiveScenePath();
                return pPath ? std::string(pPath) : std::string();
            }
            return {};
        }

        static int GetSceneCount()
        {
            if (g_SceneCallbacks.pfnGetSceneCount)
            {
                return g_SceneCallbacks.pfnGetSceneCount();
            }
            return 0;
        }

        static std::string GetScenePath(int index)
        {
            if (g_SceneCallbacks.pfnGetScenePath && index >= 0)
            {
                const char* pPath = g_SceneCallbacks.pfnGetScenePath(index);
                return pPath ? std::string(pPath) : std::string();
            }
            return {};
        }

        static std::vector<std::string> GetSceneList()
        {
            std::vector<std::string> vScenes;
            const int count = GetSceneCount();
            vScenes.reserve(static_cast<size_t>(std::max(0, count)));
            for (int i = 0; i < count; ++i)
            {
                std::string path = GetScenePath(i);
                if (!path.empty())
                {
                    vScenes.push_back(std::move(path));
                }
            }
            return vScenes;
        }

        static void DontDestroyOnLoad(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnDontDestroyOnLoad && pEntity)
            {
                g_SceneCallbacks.pfnDontDestroyOnLoad(pEntity);
            }
        }

        static void SetString(const std::string& key, const std::string& value)
        {
            if (g_SceneCallbacks.pfnSetPersistentString && !key.empty())
            {
                g_SceneCallbacks.pfnSetPersistentString(key.c_str(), value.c_str());
            }
        }

        static std::string GetString(const std::string& key, const std::string& defaultValue = "")
        {
            if (g_SceneCallbacks.pfnGetPersistentString && !key.empty())
            {
                char aBuffer[1024] = {};
                if (g_SceneCallbacks.pfnGetPersistentString(key.c_str(), aBuffer, static_cast<int>(sizeof(aBuffer))))
                {
                    return std::string(aBuffer);
                }
            }
            return defaultValue;
        }

        static void SetInt(const std::string& key, int value)
        {
            if (g_SceneCallbacks.pfnSetPersistentInt && !key.empty())
            {
                g_SceneCallbacks.pfnSetPersistentInt(key.c_str(), value);
            }
        }

        static int GetInt(const std::string& key, int defaultValue = 0)
        {
            if (g_SceneCallbacks.pfnGetPersistentInt && !key.empty())
            {
                return g_SceneCallbacks.pfnGetPersistentInt(key.c_str(), defaultValue);
            }
            return defaultValue;
        }

        static void SetFloat(const std::string& key, float value)
        {
            if (g_SceneCallbacks.pfnSetPersistentFloat && !key.empty())
            {
                g_SceneCallbacks.pfnSetPersistentFloat(key.c_str(), value);
            }
        }

        static float GetFloat(const std::string& key, float defaultValue = 0.0f)
        {
            if (g_SceneCallbacks.pfnGetPersistentFloat && !key.empty())
            {
                return g_SceneCallbacks.pfnGetPersistentFloat(key.c_str(), defaultValue);
            }
            return defaultValue;
        }

        static void SetBool(const std::string& key, bool value)
        {
            if (g_SceneCallbacks.pfnSetPersistentBool && !key.empty())
            {
                g_SceneCallbacks.pfnSetPersistentBool(key.c_str(), value);
            }
        }

        static bool GetBool(const std::string& key, bool defaultValue = false)
        {
            if (g_SceneCallbacks.pfnGetPersistentBool && !key.empty())
            {
                return g_SceneCallbacks.pfnGetPersistentBool(key.c_str(), defaultValue);
            }
            return defaultValue;
        }

        static bool HasKey(const std::string& key)
        {
            if (g_SceneCallbacks.pfnHasPersistentKey && !key.empty())
            {
                return g_SceneCallbacks.pfnHasPersistentKey(key.c_str());
            }
            return false;
        }

        static void RemoveKey(const std::string& key)
        {
            if (g_SceneCallbacks.pfnRemovePersistentKey && !key.empty())
            {
                g_SceneCallbacks.pfnRemovePersistentKey(key.c_str());
            }
        }

        static void ClearPersistentData()
        {
            if (g_SceneCallbacks.pfnClearPersistentData)
            {
                g_SceneCallbacks.pfnClearPersistentData();
            }
        }
    };

    struct SPhysicsCallbacks
    {
        bool (*pfnRaycast)(Vec3 origin, Vec3 direction, float maxDistance,
                           bool includeTriggers, int ignoreEntityId,
                           SRaycastHit* pOutHit) = nullptr;
        int (*pfnRaycastAll)(Vec3 origin, Vec3 direction, float maxDistance,
                             bool includeTriggers, int ignoreEntityId,
                             SRaycastHit* pOutHits, int maxHits) = nullptr;
        int (*pfnOverlapSphere)(Vec3 center, float radius,
                                bool includeTriggers, int ignoreEntityId,
                                CScriptEntity** ppOutEntities, int maxCount) = nullptr;
        int (*pfnOverlapBox)(Vec3 center, Vec3 halfExtents,
                             bool includeTriggers, int ignoreEntityId,
                             CScriptEntity** ppOutEntities, int maxCount) = nullptr;
        void (*pfnAddForce)(CScriptEntity* pEntity, Vec3 force) = nullptr;
        void (*pfnAddImpulse)(CScriptEntity* pEntity, Vec3 impulse) = nullptr;
        void (*pfnAddTorque)(CScriptEntity* pEntity, Vec3 torque) = nullptr;
        void (*pfnSetVelocity)(CScriptEntity* pEntity, Vec3 velocity) = nullptr;
        Vec3 (*pfnGetVelocity)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetAngularVelocity)(CScriptEntity* pEntity, Vec3 angularVelocity) = nullptr;
        Vec3 (*pfnGetAngularVelocity)(CScriptEntity* pEntity) = nullptr;
        bool (*pfnIsGrounded)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetTrigger)(CScriptEntity* pEntity, bool isTrigger) = nullptr;
        bool (*pfnIsTrigger)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetGravity)(Vec3 gravity) = nullptr;
        Vec3 (*pfnGetGravity)() = nullptr;
    };

    extern SPhysicsCallbacks g_PhysicsCallbacks;

    class Physics
    {
    public:
        static bool Raycast(const Vec3& origin,
                            const Vec3& direction,
                            float maxDistance,
                            SRaycastHit& outHit,
                            bool includeTriggers = false,
                            int ignoreEntityId = -1)
        {
            outHit = {};
            if (g_PhysicsCallbacks.pfnRaycast)
            {
                return g_PhysicsCallbacks.pfnRaycast(
                    origin, direction, maxDistance, includeTriggers, ignoreEntityId, &outHit);
            }
            return false;
        }

        static bool Raycast(const Vec3& origin,
                            const Vec3& direction,
                            float maxDistance = 1000.0f,
                            bool includeTriggers = false,
                            int ignoreEntityId = -1)
        {
            SRaycastHit hit{};
            return Raycast(origin, direction, maxDistance, hit, includeTriggers, ignoreEntityId);
        }

        static std::vector<SRaycastHit> RaycastAll(const Vec3& origin,
                                                   const Vec3& direction,
                                                   float maxDistance = 1000.0f,
                                                   bool includeTriggers = false,
                                                   int ignoreEntityId = -1)
        {
            std::vector<SRaycastHit> vHits;
            if (!g_PhysicsCallbacks.pfnRaycastAll)
            {
                return vHits;
            }
            const int count = g_PhysicsCallbacks.pfnRaycastAll(
                origin, direction, maxDistance, includeTriggers, ignoreEntityId, nullptr, 0);
            if (count <= 0)
            {
                return vHits;
            }
            vHits.resize(static_cast<size_t>(count));
            const int written = g_PhysicsCallbacks.pfnRaycastAll(
                origin, direction, maxDistance, includeTriggers, ignoreEntityId, vHits.data(), count);
            if (written < count)
            {
                vHits.resize(static_cast<size_t>(std::max(0, written)));
            }
            return vHits;
        }

        static std::vector<CScriptEntity*> OverlapSphere(const Vec3& center,
                                                         float radius,
                                                         bool includeTriggers = false,
                                                         int ignoreEntityId = -1)
        {
            std::vector<CScriptEntity*> vEntities;
            if (!g_PhysicsCallbacks.pfnOverlapSphere)
            {
                return vEntities;
            }
            const int count = g_PhysicsCallbacks.pfnOverlapSphere(
                center, radius, includeTriggers, ignoreEntityId, nullptr, 0);
            if (count <= 0)
            {
                return vEntities;
            }
            vEntities.resize(static_cast<size_t>(count), nullptr);
            const int written = g_PhysicsCallbacks.pfnOverlapSphere(
                center, radius, includeTriggers, ignoreEntityId, vEntities.data(), count);
            if (written < count)
            {
                vEntities.resize(static_cast<size_t>(std::max(0, written)));
            }
            return vEntities;
        }

        static std::vector<CScriptEntity*> OverlapBox(const Vec3& center,
                                                      const Vec3& halfExtents,
                                                      bool includeTriggers = false,
                                                      int ignoreEntityId = -1)
        {
            std::vector<CScriptEntity*> vEntities;
            if (!g_PhysicsCallbacks.pfnOverlapBox)
            {
                return vEntities;
            }
            const int count = g_PhysicsCallbacks.pfnOverlapBox(
                center, halfExtents, includeTriggers, ignoreEntityId, nullptr, 0);
            if (count <= 0)
            {
                return vEntities;
            }
            vEntities.resize(static_cast<size_t>(count), nullptr);
            const int written = g_PhysicsCallbacks.pfnOverlapBox(
                center, halfExtents, includeTriggers, ignoreEntityId, vEntities.data(), count);
            if (written < count)
            {
                vEntities.resize(static_cast<size_t>(std::max(0, written)));
            }
            return vEntities;
        }

        static void AddForce(CScriptEntity* pEntity, const Vec3& force)
        {
            if (g_PhysicsCallbacks.pfnAddForce && pEntity)
            {
                g_PhysicsCallbacks.pfnAddForce(pEntity, force);
            }
        }

        static void AddImpulse(CScriptEntity* pEntity, const Vec3& impulse)
        {
            if (g_PhysicsCallbacks.pfnAddImpulse && pEntity)
            {
                g_PhysicsCallbacks.pfnAddImpulse(pEntity, impulse);
            }
        }

        static void AddTorque(CScriptEntity* pEntity, const Vec3& torque)
        {
            if (g_PhysicsCallbacks.pfnAddTorque && pEntity)
            {
                g_PhysicsCallbacks.pfnAddTorque(pEntity, torque);
            }
        }

        static void SetVelocity(CScriptEntity* pEntity, const Vec3& velocity)
        {
            if (g_PhysicsCallbacks.pfnSetVelocity && pEntity)
            {
                g_PhysicsCallbacks.pfnSetVelocity(pEntity, velocity);
            }
        }

        static Vec3 GetVelocity(CScriptEntity* pEntity)
        {
            if (g_PhysicsCallbacks.pfnGetVelocity && pEntity)
            {
                return g_PhysicsCallbacks.pfnGetVelocity(pEntity);
            }
            return { 0.0f, 0.0f, 0.0f };
        }

        static void SetAngularVelocity(CScriptEntity* pEntity, const Vec3& angularVelocity)
        {
            if (g_PhysicsCallbacks.pfnSetAngularVelocity && pEntity)
            {
                g_PhysicsCallbacks.pfnSetAngularVelocity(pEntity, angularVelocity);
            }
        }

        static Vec3 GetAngularVelocity(CScriptEntity* pEntity)
        {
            if (g_PhysicsCallbacks.pfnGetAngularVelocity && pEntity)
            {
                return g_PhysicsCallbacks.pfnGetAngularVelocity(pEntity);
            }
            return { 0.0f, 0.0f, 0.0f };
        }

        static bool IsGrounded(CScriptEntity* pEntity)
        {
            if (g_PhysicsCallbacks.pfnIsGrounded && pEntity)
            {
                return g_PhysicsCallbacks.pfnIsGrounded(pEntity);
            }
            return false;
        }

        static void SetTrigger(CScriptEntity* pEntity, bool isTrigger)
        {
            if (g_PhysicsCallbacks.pfnSetTrigger && pEntity)
            {
                g_PhysicsCallbacks.pfnSetTrigger(pEntity, isTrigger);
            }
        }

        static bool IsTrigger(CScriptEntity* pEntity)
        {
            if (g_PhysicsCallbacks.pfnIsTrigger && pEntity)
            {
                return g_PhysicsCallbacks.pfnIsTrigger(pEntity);
            }
            return false;
        }

        static void SetGravity(const Vec3& gravity)
        {
            if (g_PhysicsCallbacks.pfnSetGravity)
            {
                g_PhysicsCallbacks.pfnSetGravity(gravity);
            }
        }

        static Vec3 GetGravity()
        {
            if (g_PhysicsCallbacks.pfnGetGravity)
            {
                return g_PhysicsCallbacks.pfnGetGravity();
            }
            return { 0.0f, -9.81f, 0.0f };
        }
    };

    struct SRenderCallbacks
    {
        void (*pfnSetMaterialColor)(CScriptEntity* pEntity, Color color) = nullptr;
        Color (*pfnGetMaterialColor)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetMaterialTexture)(CScriptEntity* pEntity, const char* pTexturePath) = nullptr;
        const char* (*pfnGetMaterialTexture)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetMaterialUVScale)(CScriptEntity* pEntity, Vec2 uvScale) = nullptr;
        Vec2 (*pfnGetMaterialUVScale)(CScriptEntity* pEntity) = nullptr;

        void (*pfnSetLightEnabled)(CScriptEntity* pEntity, bool enabled) = nullptr;
        bool (*pfnIsLightEnabled)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetLightColor)(CScriptEntity* pEntity, Color color) = nullptr;
        Color (*pfnGetLightColor)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetLightIntensity)(CScriptEntity* pEntity, float intensity) = nullptr;
        float (*pfnGetLightIntensity)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetLightRange)(CScriptEntity* pEntity, float range) = nullptr;
        float (*pfnGetLightRange)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetLightSpotAngle)(CScriptEntity* pEntity, float spotAngle) = nullptr;
        float (*pfnGetLightSpotAngle)(CScriptEntity* pEntity) = nullptr;
        void (*pfnSetLightType)(CScriptEntity* pEntity, int lightType) = nullptr;
        int (*pfnGetLightType)(CScriptEntity* pEntity) = nullptr;
    };

    extern SRenderCallbacks g_RenderCallbacks;

    class MaterialAPI
    {
    public:
        static void SetColor(CScriptEntity* pEntity, const Color& color)
        {
            if (g_RenderCallbacks.pfnSetMaterialColor && pEntity)
            {
                g_RenderCallbacks.pfnSetMaterialColor(pEntity, color);
            }
        }

        static Color GetColor(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetMaterialColor && pEntity)
            {
                return g_RenderCallbacks.pfnGetMaterialColor(pEntity);
            }
            return WHITE;
        }

        static void SetTexture(CScriptEntity* pEntity, const std::string& texturePath)
        {
            if (g_RenderCallbacks.pfnSetMaterialTexture && pEntity)
            {
                g_RenderCallbacks.pfnSetMaterialTexture(pEntity, texturePath.c_str());
            }
        }

        static std::string GetTexture(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetMaterialTexture && pEntity)
            {
                const char* pName = g_RenderCallbacks.pfnGetMaterialTexture(pEntity);
                return pName ? std::string(pName) : std::string();
            }
            return {};
        }

        static void SetUVScale(CScriptEntity* pEntity, const Vec2& uvScale)
        {
            if (g_RenderCallbacks.pfnSetMaterialUVScale && pEntity)
            {
                g_RenderCallbacks.pfnSetMaterialUVScale(pEntity, uvScale);
            }
        }

        static Vec2 GetUVScale(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetMaterialUVScale && pEntity)
            {
                return g_RenderCallbacks.pfnGetMaterialUVScale(pEntity);
            }
            return { 1.0f, 1.0f };
        }
    };

    class LightAPI
    {
    public:
        static void SetEnabled(CScriptEntity* pEntity, bool enabled)
        {
            if (g_RenderCallbacks.pfnSetLightEnabled && pEntity)
            {
                g_RenderCallbacks.pfnSetLightEnabled(pEntity, enabled);
            }
        }

        static bool IsEnabled(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnIsLightEnabled && pEntity)
            {
                return g_RenderCallbacks.pfnIsLightEnabled(pEntity);
            }
            return false;
        }

        static void SetColor(CScriptEntity* pEntity, const Color& color)
        {
            if (g_RenderCallbacks.pfnSetLightColor && pEntity)
            {
                g_RenderCallbacks.pfnSetLightColor(pEntity, color);
            }
        }

        static Color GetColor(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetLightColor && pEntity)
            {
                return g_RenderCallbacks.pfnGetLightColor(pEntity);
            }
            return WHITE;
        }

        static void SetIntensity(CScriptEntity* pEntity, float intensity)
        {
            if (g_RenderCallbacks.pfnSetLightIntensity && pEntity)
            {
                g_RenderCallbacks.pfnSetLightIntensity(pEntity, intensity);
            }
        }

        static float GetIntensity(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetLightIntensity && pEntity)
            {
                return g_RenderCallbacks.pfnGetLightIntensity(pEntity);
            }
            return 1.0f;
        }

        static void SetRange(CScriptEntity* pEntity, float range)
        {
            if (g_RenderCallbacks.pfnSetLightRange && pEntity)
            {
                g_RenderCallbacks.pfnSetLightRange(pEntity, range);
            }
        }

        static float GetRange(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetLightRange && pEntity)
            {
                return g_RenderCallbacks.pfnGetLightRange(pEntity);
            }
            return 10.0f;
        }

        static void SetSpotAngle(CScriptEntity* pEntity, float spotAngle)
        {
            if (g_RenderCallbacks.pfnSetLightSpotAngle && pEntity)
            {
                g_RenderCallbacks.pfnSetLightSpotAngle(pEntity, spotAngle);
            }
        }

        static float GetSpotAngle(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetLightSpotAngle && pEntity)
            {
                return g_RenderCallbacks.pfnGetLightSpotAngle(pEntity);
            }
            return 45.0f;
        }

        static void SetType(CScriptEntity* pEntity, EScriptLightType type)
        {
            if (g_RenderCallbacks.pfnSetLightType && pEntity)
            {
                g_RenderCallbacks.pfnSetLightType(pEntity, static_cast<int>(type));
            }
        }

        static EScriptLightType GetType(CScriptEntity* pEntity)
        {
            if (g_RenderCallbacks.pfnGetLightType && pEntity)
            {
                return static_cast<EScriptLightType>(g_RenderCallbacks.pfnGetLightType(pEntity));
            }
            return EScriptLightType::POINT;
        }
    };

    struct SDebugCallbacks
    {
        void (*pfnLog)(int level, const char* pMessage) = nullptr;
        void (*pfnDrawLine)(Vec3 start, Vec3 end, Color color, float duration) = nullptr;
        void (*pfnDrawRay)(Vec3 origin, Vec3 direction, Color color, float duration) = nullptr;
        void (*pfnDrawWireBox)(Vec3 center, Vec3 size, Color color, float duration) = nullptr;
        void (*pfnDrawWireSphere)(Vec3 center, float radius, Color color, float duration) = nullptr;
    };

    extern SDebugCallbacks g_DebugCallbacks;

    class Debug
    {
    public:
        static void Log(const std::string& message)
        {
            if (g_DebugCallbacks.pfnLog)
            {
                g_DebugCallbacks.pfnLog(0, message.c_str());
            }
        }

        static void LogWarning(const std::string& message)
        {
            if (g_DebugCallbacks.pfnLog)
            {
                g_DebugCallbacks.pfnLog(1, message.c_str());
            }
        }

        static void LogError(const std::string& message)
        {
            if (g_DebugCallbacks.pfnLog)
            {
                g_DebugCallbacks.pfnLog(2, message.c_str());
            }
        }

        static void DrawLine(const Vec3& start, const Vec3& end, const Color& color = GREEN, float duration = 0.0f)
        {
            if (g_DebugCallbacks.pfnDrawLine)
            {
                g_DebugCallbacks.pfnDrawLine(start, end, color, duration);
            }
        }

        static void DrawRay(const Vec3& origin, const Vec3& direction, const Color& color = RED, float duration = 0.0f)
        {
            if (g_DebugCallbacks.pfnDrawRay)
            {
                g_DebugCallbacks.pfnDrawRay(origin, direction, color, duration);
            }
        }

        static void DrawWireBox(const Vec3& center, const Vec3& size, const Color& color = YELLOW, float duration = 0.0f)
        {
            if (g_DebugCallbacks.pfnDrawWireBox)
            {
                g_DebugCallbacks.pfnDrawWireBox(center, size, color, duration);
            }
        }

        static void DrawWireSphere(const Vec3& center, float radius, const Color& color = SKYBLUE, float duration = 0.0f)
        {
            if (g_DebugCallbacks.pfnDrawWireSphere)
            {
                g_DebugCallbacks.pfnDrawWireSphere(center, radius, color, duration);
            }
        }
    };

    class EventBus
    {
    public:
        static std::uint64_t Subscribe(const std::string& eventName, std::function<void(const std::string&)> callback)
        {
            if (!g_SceneCallbacks.pfnSubscribeEvent || !callback)
            {
                return 0;
            }
            auto* pFn = new std::function<void(const std::string&)>(std::move(callback));
            return g_SceneCallbacks.pfnSubscribeEvent(
                eventName.c_str(),
                [](const char* pPayload, void* pData)
                {
                    auto* pCallback = static_cast<std::function<void(const std::string&)>*>(pData);
                    if (pCallback && *pCallback)
                    {
                        (*pCallback)(pPayload ? std::string(pPayload) : std::string());
                    }
                },
                [](void* pData)
                {
                    delete static_cast<std::function<void(const std::string&)>*>(pData);
                },
                pFn);
        }

        static void Unsubscribe(std::uint64_t subscriptionId)
        {
            if (g_SceneCallbacks.pfnUnsubscribeEvent && subscriptionId != 0)
            {
                g_SceneCallbacks.pfnUnsubscribeEvent(subscriptionId);
            }
        }

        static void Emit(const std::string& eventName, const std::string& payload = "")
        {
            if (g_SceneCallbacks.pfnEmitEvent)
            {
                g_SceneCallbacks.pfnEmitEvent(eventName.c_str(), payload.c_str());
            }
        }

        static void Clear()
        {
            if (g_SceneCallbacks.pfnClearEvents)
            {
                g_SceneCallbacks.pfnClearEvents();
            }
        }
    };

    class UI
    {
    public:
        static void SetText(CScriptEntity* pEntity, const std::string& text)
        {
            if (g_SceneCallbacks.pfnUiSetText && pEntity)
            {
                g_SceneCallbacks.pfnUiSetText(pEntity, text.c_str());
            }
        }

        static void SetText(const std::string& entityName, const std::string& text)
        {
            SetText(Scene::FindEntityByName(entityName), text);
        }

        static std::string GetText(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnUiGetText && pEntity)
            {
                char aBuffer[1024] = {};
                if (g_SceneCallbacks.pfnUiGetText(pEntity, aBuffer, static_cast<int>(sizeof(aBuffer))))
                {
                    return std::string(aBuffer);
                }
            }
            return "";
        }

        static void SetProgress(CScriptEntity* pEntity, float value)
        {
            if (g_SceneCallbacks.pfnUiSetProgress && pEntity)
            {
                g_SceneCallbacks.pfnUiSetProgress(pEntity, value);
            }
        }

        static void SetProgress(const std::string& entityName, float value)
        {
            SetProgress(Scene::FindEntityByName(entityName), value);
        }

        static float GetProgress(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnUiGetProgress && pEntity)
            {
                return g_SceneCallbacks.pfnUiGetProgress(pEntity);
            }
            return 0.0f;
        }

        static void SetSliderValue(CScriptEntity* pEntity, float value)
        {
            if (g_SceneCallbacks.pfnUiSetSliderValue && pEntity)
            {
                g_SceneCallbacks.pfnUiSetSliderValue(pEntity, value);
            }
        }

        static float GetSliderValue(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnUiGetSliderValue && pEntity)
            {
                return g_SceneCallbacks.pfnUiGetSliderValue(pEntity);
            }
            return 0.0f;
        }

        static void SetToggleValue(CScriptEntity* pEntity, bool checked)
        {
            if (g_SceneCallbacks.pfnUiSetToggleValue && pEntity)
            {
                g_SceneCallbacks.pfnUiSetToggleValue(pEntity, checked);
            }
        }

        static bool GetToggleValue(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnUiGetToggleValue && pEntity)
            {
                return g_SceneCallbacks.pfnUiGetToggleValue(pEntity);
            }
            return false;
        }

        static void SetVisible(CScriptEntity* pEntity, bool visible)
        {
            if (g_SceneCallbacks.pfnUiSetVisible && pEntity)
            {
                g_SceneCallbacks.pfnUiSetVisible(pEntity, visible);
            }
        }

        static void SetVisible(const std::string& entityName, bool visible)
        {
            SetVisible(Scene::FindEntityByName(entityName), visible);
        }

        static bool IsVisible(CScriptEntity* pEntity)
        {
            if (g_SceneCallbacks.pfnUiIsVisible && pEntity)
            {
                return g_SceneCallbacks.pfnUiIsVisible(pEntity);
            }
            return false;
        }

        static void SetColor(CScriptEntity* pEntity, const Color& color)
        {
            if (g_SceneCallbacks.pfnUiSetColor && pEntity)
            {
                g_SceneCallbacks.pfnUiSetColor(pEntity, color);
            }
        }

        static void SetColor(const std::string& entityName, const Color& color)
        {
            SetColor(Scene::FindEntityByName(entityName), color);
        }
    };

    class CameraAPI
    {
    public:
        static CScriptEntity* GetMainCamera()
        {
            if (g_SceneCallbacks.pfnGetMainCamera)
            {
                return g_SceneCallbacks.pfnGetMainCamera();
            }
            return nullptr;
        }

        static SRay ScreenPointToRay(const Vec2& screenPos)
        {
            if (g_SceneCallbacks.pfnScreenPointToRay)
            {
                return g_SceneCallbacks.pfnScreenPointToRay(screenPos);
            }
            return SRay{};
        }

        static float GetFov(CScriptEntity* pCamera)
        {
            if (g_SceneCallbacks.pfnGetCameraFov && pCamera)
            {
                return g_SceneCallbacks.pfnGetCameraFov(pCamera);
            }
            return 60.0f;
        }

        static void SetFov(CScriptEntity* pCamera, float fov)
        {
            if (g_SceneCallbacks.pfnSetCameraFov && pCamera)
            {
                g_SceneCallbacks.pfnSetCameraFov(pCamera, fov);
            }
        }
    };

    class Application
    {
    public:
        static void Quit()
        {
            if (g_SceneCallbacks.pfnApplicationQuit)
            {
                g_SceneCallbacks.pfnApplicationQuit();
            }
        }
    };

    inline Vec3 CScriptEntity::GetWorldPosition() const
    {
        if (g_SceneCallbacks.pfnGetWorldPosition)
        {
            return g_SceneCallbacks.pfnGetWorldPosition(const_cast<CScriptEntity*>(this));
        }
        return m_pPosition ? *m_pPosition : Vec3{ 0.0f, 0.0f, 0.0f };
    }

    inline void CScriptEntity::SetParent(CScriptEntity* pParent, bool keepWorldTransform)
    {
        if (g_SceneCallbacks.pfnSetParent)
        {
            g_SceneCallbacks.pfnSetParent(this, pParent, keepWorldTransform);
        }
    }

    inline CScriptEntity* CScriptEntity::GetParent() const
    {
        if (g_SceneCallbacks.pfnGetParent)
        {
            return g_SceneCallbacks.pfnGetParent(const_cast<CScriptEntity*>(this));
        }
        return nullptr;
    }

    inline std::vector<CScriptEntity*> CScriptEntity::GetChildren() const
    {
        std::vector<CScriptEntity*> vResult;
        if (!g_SceneCallbacks.pfnGetChildren)
        {
            return vResult;
        }
        const int total = g_SceneCallbacks.pfnGetChildren(const_cast<CScriptEntity*>(this), nullptr, 0);
        if (total <= 0)
        {
            return vResult;
        }
        vResult.resize(static_cast<size_t>(total), nullptr);
        const int written = g_SceneCallbacks.pfnGetChildren(
            const_cast<CScriptEntity*>(this), vResult.data(), total);
        vResult.resize(static_cast<size_t>(written > 0 ? written : 0));
        return vResult;
    }

    inline void CScriptEntity::SetActive(bool active)
    {
        if (g_SceneCallbacks.pfnSetEntityActive)
        {
            g_SceneCallbacks.pfnSetEntityActive(this, active);
        }
    }

    inline bool CScriptEntity::IsActive() const
    {
        if (g_SceneCallbacks.pfnIsEntityActive)
        {
            return g_SceneCallbacks.pfnIsEntityActive(const_cast<CScriptEntity*>(this));
        }
        return true;
    }

    inline IScript* CScriptEntity::GetScriptRaw() const
    {
        if (g_SceneCallbacks.pfnGetEntityScript)
        {
            return g_SceneCallbacks.pfnGetEntityScript(const_cast<CScriptEntity*>(this));
        }
        return nullptr;
    }

    inline std::uint64_t IScript::Invoke(float delaySeconds, std::function<void()> callback)
    {
        if (!g_SceneCallbacks.pfnInvokeTimer || !callback)
        {
            return 0;
        }
        auto* pFn = new std::function<void()>(std::move(callback));
        return g_SceneCallbacks.pfnInvokeTimer(
            this,
            delaySeconds,
            0.0f,
            [](void* pData)
            {
                auto* pCallback = static_cast<std::function<void()>*>(pData);
                if (pCallback && *pCallback)
                {
                    (*pCallback)();
                }
            },
            [](void* pData)
            {
                delete static_cast<std::function<void()>*>(pData);
            },
            pFn);
    }

    inline std::uint64_t IScript::InvokeRepeating(float delaySeconds, float repeatRateSeconds, std::function<void()> callback)
    {
        if (!g_SceneCallbacks.pfnInvokeTimer || !callback)
        {
            return 0;
        }
        auto* pFn = new std::function<void()>(std::move(callback));
        return g_SceneCallbacks.pfnInvokeTimer(
            this,
            delaySeconds,
            repeatRateSeconds > 0.0001f ? repeatRateSeconds : 0.0001f,
            [](void* pData)
            {
                auto* pCallback = static_cast<std::function<void()>*>(pData);
                if (pCallback && *pCallback)
                {
                    (*pCallback)();
                }
            },
            [](void* pData)
            {
                delete static_cast<std::function<void()>*>(pData);
            },
            pFn);
    }

    inline void IScript::CancelInvoke(std::uint64_t timerId)
    {
        if (g_SceneCallbacks.pfnCancelTimer && timerId != 0)
        {
            g_SceneCallbacks.pfnCancelTimer(timerId);
        }
    }

    inline void IScript::CancelAllInvokes()
    {
        if (g_SceneCallbacks.pfnCancelOwnerTimers)
        {
            g_SceneCallbacks.pfnCancelOwnerTimers(this);
        }
    }

    namespace detail
    {
        inline bool PropertyNameMatches(const std::string& requestedName, const char* pFieldName)
        {
            if (!pFieldName)
            {
                return false;
            }
            if (requestedName == pFieldName)
            {
                return true;
            }
            if (pFieldName[0] == 'm' && pFieldName[1] == '_' && requestedName == (pFieldName + 2))
            {
                return true;
            }
            return false;
        }

        template <typename TField, typename TValue>
        inline void TrySetField(const std::string& name, const char* pFieldName, TField& field, const TValue& value)
        {
            if constexpr (std::is_same_v<std::decay_t<TField>, std::decay_t<TValue>>)
            {
                if (PropertyNameMatches(name, pFieldName))
                {
                    field = value;
                }
            }
        }

        template <typename TField, typename TValue>
        inline void TryGetField(const std::string& name, const char* pFieldName, const TField& field,
                                TValue& outValue, bool& found)
        {
            if constexpr (std::is_same_v<std::decay_t<TField>, std::decay_t<TValue>>)
            {
                if (!found && PropertyNameMatches(name, pFieldName))
                {
                    outValue = field;
                    found = true;
                }
            }
        }
    } // detail

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
using Scene = ::quark::script::Scene;
using SceneManager = ::quark::script::SceneManager;
using Physics = ::quark::script::Physics;
using Debug = ::quark::script::Debug;
using EventBus = ::quark::script::EventBus;
using UI = ::quark::script::UI;
using CameraAPI = ::quark::script::CameraAPI;
using Application = ::quark::script::Application;
using SRay = ::quark::script::SRay;
using ScriptRay = ::quark::script::SRay;
using SCollisionInfo = ::quark::script::SCollisionInfo;
using CollisionInfo = ::quark::script::SCollisionInfo;
using SRaycastHit = ::quark::script::SRaycastHit;
using RaycastHit = ::quark::script::SRaycastHit;
using CScriptEntity = ::quark::script::CScriptEntity;
using ScriptEntity = ::quark::script::CScriptEntity;
using SEntityRef = ::quark::script::SEntityRef;
using SAssetRef = ::quark::script::SAssetRef;
using EntityRef = ::quark::script::SEntityRef;
using AssetRef = ::quark::script::SAssetRef;
using MaterialAPI = ::quark::script::MaterialAPI;
using LightAPI = ::quark::script::LightAPI;
using EScriptPrimitiveType = ::quark::script::EScriptPrimitiveType;
using PrimitiveType = ::quark::script::EScriptPrimitiveType;
using EScriptLightType = ::quark::script::EScriptLightType;
using ScriptLightType = ::quark::script::EScriptLightType;

#define QUARK_FIELD(member) visitor(#member, member)

#define QUARK_PROPERTIES(...)                                                                            \
    template <typename FVisitor>                                                                         \
    void VisitQuarkProperties(FVisitor&& visitor)                                                        \
    {                                                                                                    \
        __VA_ARGS__;                                                                                     \
    }                                                                                                    \
    template <typename FVisitor>                                                                         \
    void VisitQuarkProperties(FVisitor&& visitor) const                                                  \
    {                                                                                                    \
        __VA_ARGS__;                                                                                     \
    }                                                                                                    \
    void SetPropertyFloat(const std::string& name, float value) override                                 \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyInt(const std::string& name, int value) override                                     \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyBool(const std::string& name, bool value) override                                   \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyString(const std::string& name, const std::string& value) override                   \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyVec3(const std::string& name, const Vec3& value) override                            \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyColor(const std::string& name, const Color& value) override                          \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyEntityRef(const std::string& name, const ::quark::script::SEntityRef& value) override \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    void SetPropertyAssetRef(const std::string& name, const ::quark::script::SAssetRef& value) override  \
    {                                                                                                    \
        VisitQuarkProperties([&](const char* pFieldName, auto& field)                                    \
                             { ::quark::script::detail::TrySetField(name, pFieldName, field, value); }); \
    }                                                                                                    \
    bool GetPropertyFloat(const std::string& name, float& outValue) const override                       \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyInt(const std::string& name, int& outValue) const override                           \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyBool(const std::string& name, bool& outValue) const override                         \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyString(const std::string& name, std::string& outValue) const override                \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyVec3(const std::string& name, Vec3& outValue) const override                         \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyColor(const std::string& name, Color& outValue) const override                       \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyEntityRef(const std::string& name, ::quark::script::SEntityRef& outValue) const override \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }                                                                                                    \
    bool GetPropertyAssetRef(const std::string& name, ::quark::script::SAssetRef& outValue) const override \
    {                                                                                                    \
        bool found = false;                                                                              \
        VisitQuarkProperties([&](const char* pFieldName, const auto& field)                              \
                             { ::quark::script::detail::TryGetField(name, pFieldName, field, outValue, found); }); \
        return found;                                                                                    \
    }

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
