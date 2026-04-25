#include "headers/lighting.h"

static bool used[MAX_LIGHTS] = {false};

void update_lighting(Shader shader, Lighting& l) {
    l.light.position = l.position;
    l.light.target   = l.target;
    l.light.color    = l.color;
    l.light.enabled  = l.enabled;
\
    int enabled_int = l.enabled ? 1 : 0;
    SetShaderValue(shader, l.light.enabledLoc, &enabled_int, SHADER_UNIFORM_INT);

    int type = l.light.type;
    SetShaderValue(shader, l.light.typeLoc, &type, SHADER_UNIFORM_INT);

    float position[3] = { l.position.x, l.position.y, l.position.z };
    SetShaderValue(shader, l.light.positionLoc, position, SHADER_UNIFORM_VEC3);

    float target[3] = { l.target.x, l.target.y, l.target.z };
    SetShaderValue(shader, l.light.targetLoc, target, SHADER_UNIFORM_VEC3);

    float color[4] = {
        l.color.r / 255.f, l.color.g / 255.f,
        l.color.b / 255.f, l.color.a / 255.f
    };
    SetShaderValue(shader, l.light.colorLoc, color, SHADER_UNIFORM_VEC4);

    float intensity = l.intensity;
    float range = l.range;
    int intensity_loc = GetShaderLocation(shader, TextFormat("lights[%i].intensity", l.id));
    int range_loc     = GetShaderLocation(shader, TextFormat("lights[%i].range",     l.id));
    SetShaderValue(shader, intensity_loc, &intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, range_loc,     &range,     SHADER_UNIFORM_FLOAT);
}

int allocate_light_id()
{
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (!used[i]) {
            used[i] = true;
            return i;
        }
    }
    return -1;
}

void free_light_id(int id)
{
    if (id >= 0 && id < MAX_LIGHTS)
        used[id] = false;
}

Lighting create_lighting(Vector3 pos, Color color)
{
    Lighting l = {};
    l.position = pos;
    l.target = Vector3Zero();
    l.color = color;
    l.enabled = true;
    l.intensity = 1.0f;
    l.range = 5.0f;
    return l;
}

Light create_light_at_slot(int slot, int type, Vector3 position, Vector3 target, Color color, Shader shader)
{
    Light light = { 0 };
    light.enabled  = true;
    light.type     = type;
    light.position = position;
    light.target   = target;
    light.color    = color;

    light.enabledLoc  = GetShaderLocation(shader, TextFormat("lights[%i].enabled",  slot));
    light.typeLoc     = GetShaderLocation(shader, TextFormat("lights[%i].type",      slot));
    light.positionLoc = GetShaderLocation(shader, TextFormat("lights[%i].position",  slot));
    light.targetLoc   = GetShaderLocation(shader, TextFormat("lights[%i].target",    slot));
    light.colorLoc    = GetShaderLocation(shader, TextFormat("lights[%i].color",     slot));

    return light;
}
