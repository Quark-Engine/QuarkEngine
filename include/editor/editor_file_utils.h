#ifndef __EDITOR_FILE_UTILS_H__
#define __EDITOR_FILE_UTILS_H__

#include <string>
#include <vector>

bool MoveFile(const std::string& source, const std::string& destination);

bool MoveDirectory(const std::string& source, const std::string& destination);

bool CopyFile(const std::string& source, const std::string& destination);

bool CopyDirectory(const std::string& source, const std::string& destination);

std::vector<std::string> GetDirectoryContents(const std::string& path);

bool IsDirectory(const std::string& path);

bool IsFile(const std::string& path);

bool DeleteFileOrDirectory(const std::string& path);

#endif // __EDITOR_FILE_UTILS_H__
