#pragma once

#include "raylib.h"
#include "../headers/editor.h"
#include "../headers/camera.h"
#include "../headers/entity.h"

struct MeshEditState {
    bool enabled = false;
    int entity_index = -1;
    int mesh_index = 0;
    int triangle_index = 0;
    int vertex_corner = 0;
    bool was_using_gizmo = false;
};

extern MeshEditState g_mesh_edit_state;

void draw_ui(Editor& editor, Shader shader, FlyCamera camera);

void draw_gizmo(Editor& editor, FlyCamera camera);
void handle_scene_asset_drop(Editor& editor, Camera3D camera, bool is_hovered);

void draw_mesh_vertex_overlay(Editor& editor, Camera3D camera);
void reset_mesh_edit_model(Entity& entity);
