#pragma once

#include <string>
#include <vector>

bool move_file(const std::string& source, const std::string& destination);

bool move_directory(const std::string& source, const std::string& destination);

bool copy_file(const std::string& source, const std::string& destination);

bool copy_directory(const std::string& source, const std::string& destination);

std::vector<std::string> get_directory_contents(const std::string& path);

bool is_directory(const std::string& path);

bool is_file(const std::string& path);

bool delete_file_or_directory(const std::string& path);
