#ifndef __SCRIPT_ENGINE_H__
#define __SCRIPT_ENGINE_H__

#include "dynamic_library.h"
#include "scene.h"
#include "scripting/script_component.h"
#include <functional>
#include <memory>
#include <string>
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

        static CScriptEngine& Get();

        bool BuildScriptDll(const std::string& projectPath, const std::string& gameName,
                            std::string& errorMessage,
                            const FProgressCallback& progressCallback = nullptr);

        bool LoadScriptDll(const std::string& dllPath);
        void UnloadScriptDll();
        bool IsDllLoaded() const;

        IScript* CreateScript(const std::string& scriptName);
        void DestroyScript(const std::string& scriptName, IScript* pScript);

        void StartPlay(CScene& scene, const std::string& projectPath,
                       CDynamicLibrary* pExternalDll = nullptr);
        void UpdatePlay(CScene& scene, float deltaTime);
        void StopPlay(CScene& scene);

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

    private:
        CScriptEngine();
        ~CScriptEngine();

        CScriptEngine(const CScriptEngine&) = delete;
        CScriptEngine& operator=(const CScriptEngine&) = delete;

        CDynamicLibrary m_Dll;
        CDynamicLibrary* m_pActiveDll = nullptr;
        CScene* m_pActiveScene = nullptr;
        std::string m_ActiveProjectPath;
        float m_FixedAccumulator = 0.0f;
        bool m_IsPlaying = false;
        bool m_ViewportHovered = true;
        bool m_CursorLocked = false;
        std::string m_LoadedDllPath;
        std::vector<SParsedScriptInfo> m_vAvailableScripts;
        std::vector<int> m_vPendingDestroyIds;
        std::string m_PendingLoadScenePath;
        std::vector<SScriptLogEntry> m_vLogs;
    };

} // quark::script

#endif // __SCRIPT_ENGINE_H__
