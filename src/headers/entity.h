#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "lighting.h"
#include <string>
#include <functional>

enum ObjectType { CUBE, SPHERE, CONE, CYLINDER, HEMISPHERE, TORUS };

inline const char* object_type_name(ObjectType type) {
    switch (type) {
        case CUBE: return "Cube";
        case SPHERE: return "Sphere";
        case CONE: return "Cone";
        case CYLINDER: return "Cylinder";
        case HEMISPHERE: return "HemiSphere";
        case TORUS: return "Torus";
        default: return "Object";
    }
}

struct ModelAsset {
    std::string name;
    std::string filepath;
    
    ObjectType type;
    bool is_procedural;
    std::function<Model(int)> generator;
    Model loaded_model = {0};
};

enum TextureSource {
    TEXTURE_NONE,
    TEXTURE_EXTERNAL,
    TEXTURE_MODEL
};

struct Entity {
    int id;
    std::string name;
    
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;

    Texture2D texture = {0};
    TextureSource texture_source = TEXTURE_NONE;
    std::string texture_name;
    std::vector<Texture2D> original_material_textures;

    bool auto_uv;
    bool texture_stretch = true; 

    Model model = {0};
    bool owns_model_instance = false;
    ModelAsset* asset;
    std::string asset_name;

    float texture_repeat_u;
    float texture_repeat_v;
    Vector2 uv_scale_vec;
    float uv_scale;
    std::vector<std::vector<float>> original_texcoords;

    int segments;

    ObjectType type;

    Color color;
    Color outline_color;

    bool has_light = false;
    bool light_created = false;
    bool shader_assigned = false;
    bool owns_materials = false;
    Lighting light;

    Entity()
        : id(0),
          position{0, 0, 0},
          rotation{0, 0, 0},
          scale{1, 1, 1},
          auto_uv(false),
          texture_repeat_u(1.0f),
          texture_repeat_v(1.0f),
          uv_scale(1.0f),
          uv_scale_vec{1, 1},
          color(WHITE),
          outline_color(LIGHTGRAY),
          asset(nullptr),
          segments(16),
          type(CUBE)
    {}

    Entity(int _id)
        : id(_id),
          position{0, 0, 0},
          rotation{0, 0, 0},
          scale{1, 1, 1},
          auto_uv(false),
          texture_repeat_u(1.0f),
          texture_repeat_v(1.0f),
          uv_scale(1.0f),
          uv_scale_vec{1, 1},
          color(WHITE),
          outline_color(LIGHTGRAY),
          asset(nullptr),
          segments(16),
          type(CUBE)
    {
        name = object_type_name(type);
    }
};
