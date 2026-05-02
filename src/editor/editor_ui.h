#pragma once

#include "raylib.h"
#include "../headers/editor.h"

void draw_ui(Editor& editor, Shader shader, Camera3D camera);

void draw_gizmo(Editor& editor, Camera3D camera);
void handle_scene_asset_drop(Editor& editor, Camera3D camera, bool is_hovered);
