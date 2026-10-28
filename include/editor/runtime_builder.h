#ifndef __EDITOR_RUNTIME_BUILDER_H__
#define __EDITOR_RUNTIME_BUILDER_H__

#include "editor/editor_preferences.h"
#include "project.h"
#include <functional>
#include <string>

class CRuntimeBuilder
{
public:
    using FProgressCallback = std::function<void(const std::string&)>;

    static bool Build(const std::string& projectPath, const std::string& sceneDocument,
                      const CPreferences& preferences,
                      const SProjectBuildSettings& buildSettings,
                      std::string& outputPath, std::string& errorMessage,
                      const FProgressCallback& progressCallback = {});
    static bool BuildAndRun(const std::string& projectPath, const std::string& sceneDocument,
                            const CPreferences& preferences,
                            const SProjectBuildSettings& buildSettings,
                            std::string& outputPath, std::string& errorMessage,
                            const FProgressCallback& progressCallback = {});
};

#endif // __EDITOR_RUNTIME_BUILDER_H__
