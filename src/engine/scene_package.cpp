#include "engine/scene_package.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace quark
{

namespace
{

constexpr char PACKAGE_MAGIC[] = { 'Q', 'S', 'C', 'N', 'P', 'A', 'K', '1' };
constexpr std::uint64_t MAX_ENTRY_SIZE = 256ull * 1024ull * 1024ull;
constexpr std::uint32_t MAX_SCENE_COUNT = 100000;

bool IsSafeRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name())
    {
        return false;
    }
    for (const std::filesystem::path& component : path)
    {
        if (component == ".." || component == ".")
        {
            return false;
        }
    }
    return true;
}

} // anonymous

bool CScenePackage::Write(const std::filesystem::path& packagePath,
                          const std::string& activeScene,
                          const std::filesystem::path& projectRoot,
                          const std::filesystem::path& activeScenePath,
                          std::string& errorMessage)
{
    if (activeScene.size() > MAX_ENTRY_SIZE || !IsSafeRelativePath(activeScenePath))
    {
        errorMessage = "Active scene path or data is invalid";
        return false;
    }
    std::vector<std::pair<std::string, std::string>> vEntries;
    vEntries.emplace_back(activeScenePath.generic_string(), activeScene);
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(projectRoot, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end)
    {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::path relativePath =
            std::filesystem::relative(entry.path(), projectRoot, error);
        if (error)
        {
            break;
        }
        if (entry.is_directory() &&
            (relativePath == "Build" || relativePath == "resources"))
        {
            iterator.disable_recursion_pending();
        }
        if (relativePath.empty() || *relativePath.begin() == "Build" ||
            *relativePath.begin() == "resources")
        {
            iterator.increment(error);
            continue;
        }
        if (!entry.is_regular_file() || entry.path().extension() != ".json" ||
            relativePath == activeScenePath)
        {
            iterator.increment(error);
            continue;
        }

        std::ifstream sceneFile(entry.path(), std::ios::binary);
        if (!sceneFile)
        {
            errorMessage = "Failed to read scene: " + entry.path().string();
            return false;
        }
        const std::string sceneDocument { std::istreambuf_iterator<char> { sceneFile },
            std::istreambuf_iterator<char> {} };
        if (sceneDocument.size() > MAX_ENTRY_SIZE)
        {
            errorMessage = "Scene is too large to package: " + entry.path().string();
            return false;
        }
        vEntries.emplace_back(relativePath.generic_string(), sceneDocument);
        iterator.increment(error);
    }
    if (error)
    {
        errorMessage = "Failed to enumerate project scenes: " + error.message();
        return false;
    }
    if (vEntries.size() > MAX_SCENE_COUNT)
    {
        errorMessage = "Project contains too many scenes";
        return false;
    }

    return WriteFiles(packagePath, vEntries, errorMessage);
}

bool CScenePackage::WriteFiles(const std::filesystem::path& packagePath,
                               const std::vector<std::pair<std::string, std::string>>& vEntries,
                               std::string& errorMessage)
{
    if (vEntries.empty() || vEntries.size() > MAX_SCENE_COUNT)
    {
        errorMessage = "Package contains an invalid number of entries";
        return false;
    }

    for (const auto& [entryName, entryData] : vEntries)
    {
        if (!IsSafeRelativePath(entryName) || entryName.size() > 4096 ||
            entryData.size() > MAX_ENTRY_SIZE)
        {
            errorMessage = "Package contains an invalid or oversized entry: " + entryName;
            return false;
        }
    }

    std::ofstream packageFile(packagePath, std::ios::binary | std::ios::trunc);
    if (!packageFile)
    {
        errorMessage = "Failed to create package file";
        return false;
    }

    const std::uint32_t entryCount = static_cast<std::uint32_t>(vEntries.size());
    packageFile.write(PACKAGE_MAGIC, sizeof(PACKAGE_MAGIC));
    packageFile.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
    for (const auto& [entryName, entryData] : vEntries)
    {
        const std::uint32_t nameSize = static_cast<std::uint32_t>(entryName.size());
        const std::uint64_t dataSize = static_cast<std::uint64_t>(entryData.size());
        packageFile.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
        packageFile.write(entryName.data(), static_cast<std::streamsize>(entryName.size()));
        packageFile.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
        packageFile.write(entryData.data(), static_cast<std::streamsize>(entryData.size()));
    }
    if (!packageFile)
    {
        errorMessage = "Failed to write package file";
        return false;
    }
    return true;
}

bool CScenePackage::Extract(const std::filesystem::path& packagePath,
                            const std::filesystem::path& projectRoot)
{
    std::ifstream packageFile(packagePath, std::ios::binary);
    if (!packageFile)
    {
        return false;
    }

    char aMagic[sizeof(PACKAGE_MAGIC)] = {};
    std::uint32_t entryCount = 0;
    packageFile.read(aMagic, sizeof(aMagic));
    packageFile.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
    if (!packageFile || std::memcmp(aMagic, PACKAGE_MAGIC, sizeof(aMagic)) != 0 ||
        entryCount == 0 || entryCount > MAX_SCENE_COUNT)
    {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(projectRoot, error);
    if (error)
    {
        return false;
    }
    for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
    {
        std::uint32_t nameSize = 0;
        std::uint64_t dataSize = 0;
        packageFile.read(reinterpret_cast<char*>(&nameSize), sizeof(nameSize));
        if (!packageFile || nameSize == 0 || nameSize > 4096)
        {
            return false;
        }
        std::string name(nameSize, '\0');
        packageFile.read(name.data(), static_cast<std::streamsize>(name.size()));
        packageFile.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
        if (!packageFile || dataSize > MAX_ENTRY_SIZE)
        {
            return false;
        }
        const std::filesystem::path relativePath(name);
        if (!IsSafeRelativePath(relativePath) ||
            relativePath.generic_string().find(':') != std::string::npos)
        {
            return false;
        }

        std::string entryData(static_cast<std::size_t>(dataSize), '\0');
        packageFile.read(entryData.data(), static_cast<std::streamsize>(entryData.size()));
        if (!packageFile)
        {
            return false;
        }
        const std::filesystem::path destination = projectRoot / relativePath;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error)
        {
            return false;
        }
        std::ofstream entryFile(destination, std::ios::binary | std::ios::trunc);
        entryFile.write(entryData.data(), static_cast<std::streamsize>(entryData.size()));
        if (!entryFile)
        {
            return false;
        }
    }
    return true;
}

} // quark
