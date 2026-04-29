#pragma once
#include "scene.h"
#include "raylib.h"
#include <stack>
#include <filesystem>

struct SceneState {
    std::vector<Entity> entities;
    int selected;
};

struct Editor {
    Scene scene;
    std::string project_path = "projects/TestProject";
    int selected_asset_index = -1;

    std::stack<SceneState> undo_stack;
    std::stack<SceneState> redo_stack;

    std::filesystem::path current_asset_path;
    
    void handle_input();
    void handle_scene_asset_drop(Camera3D camera);
    void save_state();
    void undo();
    void redo();
};
