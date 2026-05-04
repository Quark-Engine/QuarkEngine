#pragma once
#include "scene.h"
#include "camera.h"
#include "raylib.h"
#include "tex.h"
#include "models.h"
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
    
    void draw_ui(Shader shader, FlyCamera camera);
    void draw_assets_ui();
    void handle_input();
    void draw_entity_with_texture(Entity& e);
    void save_state();
    void undo();
    void redo();
};
