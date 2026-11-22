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

    private:
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
    };

} // quark::script

#endif // __SCRIPT_ENGINE_H__
