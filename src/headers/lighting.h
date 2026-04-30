#pragma once
#include "raylib.h"
#include "rlights.h"
#include <string>

#define LIGHT_POINT       0
#define LIGHT_DIRECTIONAL 1
#define LIGHT_SPOT        2
#define LIGHT_AREA        3

struct Lighting {
    int id = -1;

    Light light;
    Vector3 position;
    Vector3 target;
    Vector3 rotation;

    Color color = WHITE;
    bool enabled;

    float spot_angle;
    float spot_angle_loc = -1;

    float intensity = 1.0f;
    float range = 5.0f;
};

Lighting create_lighting(Vector3 pos, Color color);
Light create_light_at_slot(int slot, int type, Vector3 position, Vector3 target, Color color, Shader shader);
void update_lighting(Shader shader, Lighting& l);
void free_light_id(int id);
int allocate_light_id();
