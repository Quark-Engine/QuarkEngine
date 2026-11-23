#ifndef __SCRIPT_ENGINE_H__
#define __SCRIPT_ENGINE_H__

#include "dynamic_library.h"
#include "engine/physics_world.h"
#include "scene.h"
#include "scripting/script_component.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace quark::script
{

    enum class EScriptLogLevel
    {
        INFO = 0,
        WARNING = 1,
        ERR = 2
    };

    struct SScriptLogEntry
    {
        EScriptLogLevel m_Level = EScriptLogLevel::INFO;
        std::string m_Message;
        double m_Timestamp = 0.0;
    };

    enum class EScriptDebugDrawType
    {
        LINE = 0,
        WIRE_BOX = 1,
        WIRE_SPHERE = 2
    };

    struct SScriptDebugDrawCommand
    {
        EScriptDebugDrawType m_Type = EScriptDebugDrawType::LINE;
        Vec3 m_A = { 0.0f, 0.0f, 0.0f };
        Vec3 m_B = { 0.0f, 0.0f, 0.0f };
        float m_Radius = 0.5f;
        Color m_Color = GREEN;
        float m_RemainingSeconds = 0.0f;
    };

    struct SScriptTimerEntry
    {
        std::uint64_t m_Id = 0;
        IScript* m_pOwner = nullptr;
        float m_Remaining = 0.0f;
        float m_RepeatInterval = 0.0f;
        void (*m_pfnInvoke)(void*) = nullptr;
        void (*m_pfnDestroy)(void*) = nullptr;
        void* m_pUserData = nullptr;
        bool m_Cancelled = false;
    };

    struct SScriptEventSubscription
    {
        std::uint64_t m_Id = 0;
        std::string m_EventName;
        void (*m_pfnInvoke)(const char*, void*) = nullptr;
        void (*m_pfnDestroy)(void*) = nullptr;
        void* m_pUserData = nullptr;
    };

    struct SParsedScriptInfo
    {
        std::string m_ClassName;
        std::string m_HeaderPath;
        std::vector<SScriptField> m_vFields;
    };

    class CScriptHeaderParser
    {
    public:
        static bool ParseHeader(const std::string& filePath, SParsedScriptInfo& outInfo);
        static std::vector<SParsedScriptInfo> ScanProjectScripts(const std::string& projectPath);
    };

    class CScriptEngine
    {
    public:
        using FProgressCallback = std::function<void(const std::string&)>;
        using FSceneLoadedCallback = std::function<void(CScene&)>;

        static CScriptEngine& Get();

        bool BuildScriptDll(const std::string& projectPath, const std::string& gameName,
                            std::string& errorMessage,
                            const FProgressCallback& progressCallback = nullptr,
                            bool releaseBuild = false);

        bool LoadScriptDll(const std::string& dllPath);
        void UnloadScriptDll();
        bool IsDllLoaded() const;

        IScript* CreateScript(const std::string& scriptName);
        void DestroyScript(const std::string& scriptName, IScript* pScript);

        void StartPlay(CScene& scene, const std::string& projectPath,
                       CDynamicLibrary* pExternalDll = nullptr,
                       const std::string& activeScenePath = "",
                       FSceneLoadedCallback onSceneLoaded = nullptr);
        void UpdatePlay(CScene& scene, float deltaTime);
        void StopPlay(CScene& scene);

        bool RequestLoadScene(const std::string& scenePath);
        bool ReloadActiveScene();
        bool FlushPendingSceneLoad(CScene& scene);
        const std::string& GetActiveScenePath() const
        {
            return m_ActiveScenePath;
        }
        const std::string& GetActiveSceneName() const
        {
            return m_ActiveSceneName;
        }
        const std::string& GetActiveProjectPath() const
        {
            return m_ActiveProjectPath;
        }
        void MarkDontDestroyOnLoad(int entityId);
        void ClearPersistentData();
        void SetPersistentString(const std::string& key, const std::string& value);
        bool GetPersistentString(const std::string& key, std::string& outValue) const;
        void SetPersistentInt(const std::string& key, int value);
        int GetPersistentInt(const std::string& key, int defaultValue) const;
        void SetPersistentFloat(const std::string& key, float value);
        float GetPersistentFloat(const std::string& key, float defaultValue) const;
        void SetPersistentBool(const std::string& key, bool value);
        bool GetPersistentBool(const std::string& key, bool defaultValue) const;
        bool HasPersistentKey(const std::string& key) const;
        void RemovePersistentKey(const std::string& key);

        bool IsPlaying() const
        {
            return m_IsPlaying;
        }

        const std::vector<SParsedScriptInfo>& GetAvailableScripts() const
        {
            return m_vAvailableScripts;
        }

        void RefreshAvailableScripts(const std::string& projectPath);

        std::string FindScriptHeaderPath(const std::string& scriptName) const;

        void SetViewportHovered(bool hovered)
        {
            m_ViewportHovered = hovered;
        }

        bool IsViewportHovered() const
        {
            return m_ViewportHovered;
        }

        void SetCursorLocked(bool locked);

        bool IsCursorLocked() const
        {
            return m_CursorLocked;
        }

        void LogMessage(EScriptLogLevel level, const std::string& message, bool emitToTraceLog = true);
        std::vector<SScriptLogEntry> GetLogs() const;
        void ClearLogs();

        CScene* GetActiveScene() const
        {
            return m_pActiveScene;
        }

        quark::physics::CPhysicsWorld& GetPhysicsWorld()
        {
            return m_PhysicsWorld;
        }

        void SetPhysicsSettings(const Vec3& gravity, float fixedTimestep);

        void SetActiveViewportRect(const Vec2& pos, const Vec2& size)
        {
            m_ViewportPos = pos;
            m_ViewportSize = size;
        }
        Vec2 GetActiveViewportPos() const
        {
            return m_ViewportPos;
        }
        Vec2 GetActiveViewportSize() const
        {
            return m_ViewportSize;
        }

        void RenderDebugDraw() const;
        void AddDebugDrawCommand(const SScriptDebugDrawCommand& command);
        void ClearDebugDraw();
        size_t GetDebugDrawCount() const
        {
            return m_vDebugDrawCommands.size();
        }

        std::uint64_t AddTimer(IScript* pOwner, float delay, float repeatInterval,
                               void (*pfnInvoke)(void*), void (*pfnDestroy)(void*), void* pUserData);
        void CancelTimer(std::uint64_t timerId);
        void CancelOwnerTimers(IScript* pOwner);
        void ClearAllTimers();

        std::uint64_t SubscribeEvent(const char* pEventName,
                                     void (*pfnInvoke)(const char*, void*),
                                     void (*pfnDestroy)(void*),
                                     void* pUserData);
        void UnsubscribeEvent(std::uint64_t subscriptionId);
        void EmitEvent(const char* pEventName, const char* pPayload);
        void ClearEvents();

        void RequestApplicationQuit()
        {
            m_QuitRequested = true;
        }
        bool ConsumeQuitRequested()
        {
            const bool requested = m_QuitRequested;
            m_QuitRequested = false;
            return requested;
        }

    private:
        void UpdateTimers(float deltaTime);
        void UpdateDebugDraw(float deltaTime);
        CScriptEngine();
        ~CScriptEngine();

        CScriptEngine(const CScriptEngine&) = delete;
        CScriptEngine& operator=(const CScriptEngine&) = delete;

        bool ExecuteLoadScene(CScene& scene, const std::string& resolvedScenePath);
        std::string ResolveSceneFilePath(const std::string& scenePath) const;
        void UpdateActiveSceneIdentity(const std::string& resolvedScenePath);

        CDynamicLibrary m_Dll;
        CDynamicLibrary* m_pActiveDll = nullptr;
        CScene* m_pActiveScene = nullptr;
        FSceneLoadedCallback m_OnSceneLoaded;
        quark::physics::CPhysicsWorld m_PhysicsWorld;
        std::string m_ActiveProjectPath;
        std::string m_ActiveScenePath;
        std::string m_ActiveSceneName;
        float m_FixedAccumulator = 0.0f;
        float m_ConfiguredFixedTimestep = 0.02f;
        bool m_IsPlaying = false;
        bool m_InUpdatePlay = false;
        bool m_ViewportHovered = true;
        bool m_CursorLocked = false;
        std::string m_LoadedDllPath;
        std::vector<SParsedScriptInfo> m_vAvailableScripts;
        std::vector<int> m_vPendingDestroyIds;
        std::string m_PendingLoadScenePath;
        std::vector<std::string> m_vCachedSceneList;
        std::unordered_set<int> m_DontDestroyEntityIds;
        std::unordered_map<std::string, std::string> m_PersistentStrings;
        std::unordered_map<std::string, int> m_PersistentInts;
        std::unordered_map<std::string, float> m_PersistentFloats;
        std::unordered_map<std::string, bool> m_PersistentBools;
        std::vector<SScriptLogEntry> m_vLogs;
        std::vector<SScriptTimerEntry> m_vTimers;
        std::vector<SScriptEventSubscription> m_vEventSubscriptions;
        std::vector<SScriptDebugDrawCommand> m_vDebugDrawCommands;
        std::uint64_t m_NextTimerId = 1;
        std::uint64_t m_NextEventId = 1;
        Vec2 m_ViewportPos = { 0.0f, 0.0f };
        Vec2 m_ViewportSize = { 1280.0f, 720.0f };
        bool m_QuitRequested = false;
    };

} // quark::script

#endif // __SCRIPT_ENGINE_H__
