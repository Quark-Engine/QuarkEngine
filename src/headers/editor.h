#pragma once
#include "scene.h"
#include "raylib.h"
#include "tex.h"
#include "models.h"
#include <stack>

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
    
    void draw_ui(Shader shader);
    void draw_assets_ui();
    void handle_input();
    void handle_scene_asset_drop(Camera3D camera);
    void draw_entity_with_texture(Entity& e);
    void draw_gizmo(Camera3D camera);
    void save_state();
    void undo();
    void redo();
};
