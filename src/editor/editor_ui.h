#pragma once

#include "raylib.h"
#include "../headers/editor.h"

void draw_ui(Editor& editor, Shader shader);

void draw_gizmo(Editor& editor, Camera3D camera);
void handle_scene_asset_drop(Editor& editor, Camera3D camera);

