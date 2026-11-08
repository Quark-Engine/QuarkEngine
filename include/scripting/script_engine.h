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

struct SParsedScriptInfo
{
    std::string className;
    std::string headerPath;
    std::vector<SScriptField> vFields;
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

private:
    CScriptEngine();
    ~CScriptEngine();

    CScriptEngine(const CScriptEngine&) = delete;
    CScriptEngine& operator=(const CScriptEngine&) = delete;

    CDynamicLibrary m_Dll;
    CDynamicLibrary* m_pActiveDll = nullptr;
    bool m_IsPlaying = false;
    bool m_ViewportHovered = true;
    bool m_CursorLocked = false;
    std::string m_LoadedDllPath;
    std::vector<SParsedScriptInfo> m_vAvailableScripts;
};

} // quark::script

#endif // __SCRIPT_ENGINE_H__
