#ifndef __EDITOR_RUNTIME_BUILDER_H__
#define __EDITOR_RUNTIME_BUILDER_H__

#include "scene.h"
#include "editor/editor_preferences.h"
#include <string>

class CRuntimeBuilder
{
public:
    static bool Build(const std::string& projectPath, const CScene& scene,
                      const CPreferences& preferences,
                      std::string& outputPath, std::string& errorMessage);
    static bool BuildAndRun(const std::string& projectPath, const CScene& scene,
                            const CPreferences& preferences,
                            std::string& outputPath, std::string& errorMessage);
};

#endif // __EDITOR_RUNTIME_BUILDER_H__
