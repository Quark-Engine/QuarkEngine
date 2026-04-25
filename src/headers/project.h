#pragma once
#include <string>
#include "scene.h"

void project_new(const std::string& folder_path, Scene& scene);
void project_save(const std::string& folder_path, const Scene& scene);
bool project_load(const std::string& folder_path, Scene& scene, Shader shader);
