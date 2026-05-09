#pragma once

#include "raylib.h"
#include "../headers/editor.h"
#include "../headers/camera.h"
#include "../headers/entity.h"
#include "editor_hierarchy_utils.h"
#include "editor_file_utils.h"

struct MeshEditState {
    bool enabled = false;
    int entity_index = -1;
    int mesh_index = 0;
    int triangle_index = 0;
    int vertex_corner = 0;
    bool was_using_gizmo = false;
};

enum PolygonEditMode {
    POLY_NONE,
    POLY_CREATE,
    POLY_MOVE
};

extern MeshEditState g_mesh_edit_state;
extern PolygonEditMode g_poly_mode;
extern std::vector<int> g_selected_vertices;

void copy_entity(Entity* entity);
void paste_entity(Editor& editor);
void dublicate_entity(Editor& editor, Entity* entity);
void delete_entity(Editor& editor, Entity* entity, Shader shader);

void draw_ui(Editor& editor, Shader shader, FlyCamera camera);

void draw_gizmo(Editor& editor, FlyCamera camera);
void handle_scene_asset_drop(Editor& editor, Camera3D camera);

void draw_mesh_vertex_overlay(Editor& editor, Camera3D camera);
void reset_mesh_edit_model(Entity& entity);
