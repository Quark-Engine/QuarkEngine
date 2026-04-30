#pragma once
#include "scene.h"
#include "raylib.h"
#include <stack>
#include <filesystem>

struct SceneState {
    std::vector<Entity> entities;
    int selected;
};

extern Scene scene;
extern std::string project_path;
extern int selected_asset_index;

extern std::stack<SceneState> undo_stack;
extern std::stack<SceneState> redo_stack;

extern std::filesystem::path current_asset_path;

void handle_input();
void handle_scene_asset_drop(Camera3D camera);
void save_state();
void undo();
void redo();