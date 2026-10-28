#ifndef __ENGINE_SCENE_PACKAGE_H__
#define __ENGINE_SCENE_PACKAGE_H__

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace quark
{

class CScenePackage
{
public:
    static bool Write(const std::filesystem::path& packagePath,
                      const std::string& activeScene,
                      const std::filesystem::path& projectRoot,
                      const std::filesystem::path& activeScenePath,
                      std::string& errorMessage);
    static bool WriteFiles(const std::filesystem::path& packagePath,
                           const std::vector<std::pair<std::string, std::string>>& vEntries,
                           std::string& errorMessage);
    static bool Extract(const std::filesystem::path& packagePath,
                        const std::filesystem::path& projectRoot);
};

} // quark

#endif // __ENGINE_SCENE_PACKAGE_H__
