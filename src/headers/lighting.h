#pragma once
#include "QuarkCore/QuarkCore.hpp"
#include "QuarkCore/QuarkLights.hpp"
using namespace qc;

#include <string>

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_AREA        3

struct Lighting {
    int id = -1;

    Light light;
    Vec3 position;
    Vec3 target;
    Vec3 rotation;

    Color color = WHITE;
    bool enabled;

    float spot_angle;
    int spot_angle_loc = -1;
    int intensity_loc = -1;
    int range_loc = -1;

    float intensity = 1.0f;
    float range = 5.0f;
};

struct ShadowMap {
    RenderTexture2D fbo = {0};
    int size = 2048;
    Mat4 light_space = Mat4::identity();
    bool active = false;

    void init(int resolution = 2048);
    void unload();
};

// lighting
Lighting create_lighting(Vec3 pos, Color color);
Light create_light_at_slot(int slot, int type, Vec3 position, Vec3 target, Color color, Shader shader);
void initialize_lighting_uniform_cache(Lighting& lighting, Shader shader, int slot);
void update_lighting(Shader shader, Lighting& l);
void free_light_id(int id);
int allocate_light_id();
void reset_light_registry();

// shadows
void shadow_map_begin(ShadowMap& sm);
void shadow_map_end();

Mat4 compute_light_space_matrix(Vec3 light_pos, Vec3 target, float ortho_size, float near, float far);
Mat4 compute_spot_light_space_matrix(Vec3 light_pos, Vec3 target, float fov, float near, float far);