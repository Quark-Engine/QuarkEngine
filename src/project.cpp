#include "headers/project.h"
#include "headers/entity.h"
#include "headers/lighting.h"
#include "headers/models.h"
#include "headers/tex.h"
#include "raylib.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string color_to_hex(Color color) {
    char buf[12];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return buf;
}

static Color hex_to_color(const std::string& hex) {
    Color color = WHITE;
    if (hex.size() < 7) return color;

    color.r = (unsigned char)strtol(hex.substr(1, 2).c_str(), nullptr, 16);
    color.g = (unsigned char)strtol(hex.substr(3, 2).c_str(), nullptr, 16);
    color.b = (unsigned char)strtol(hex.substr(5, 2).c_str(), nullptr, 16);
    color.a = hex.size() >= 9 ? (unsigned char)strtol(hex.substr(7, 2).c_str(), nullptr, 16) : 255;
    return color;
}

void project_new(const std::string& folder_path, Scene& scene) {
    fs::create_directories(folder_path);
    fs::create_directories(folder_path + "/resources");

    json j;
    j["entities"] = json::array();
    
    std::ofstream f(folder_path + "/scene.json");
    f << j.dump(4);

    scene.release_resources();
}

void project_save(const std::string& folder_path, const Scene& scene) {
    fs::create_directories(folder_path + "/resources");

    json j;
    j["entities"] = json::array();

    for (const auto& e : scene.entities) {
        json ej;
        ej["name"]          = e.name;
        ej["type"]          = e.type;

        ej["position"] = json::array({ (float)e.position.x, (float)e.position.y, (float)e.position.z });
        ej["rotation"] = json::array({ (float)e.rotation.x, (float)e.rotation.y, (float)e.rotation.z });
        ej["scale"]    = json::array({ (float)e.scale.x,    (float)e.scale.y,    (float)e.scale.z    });

        ej["color"]         = color_to_hex(e.color);
        ej["outline_color"] = color_to_hex(e.outline_color);

        std::string tex_name = "None";
        if (e.texture_source == TEXTURE_EXTERNAL) {
            tex_name = e.texture_name.empty() ? "None" : e.texture_name;
        } else if (e.texture_source == TEXTURE_MODEL) {
            tex_name = "__model__";
        }

        ej["texture"]          = tex_name;
        ej["texture_source"]   = (e.texture_source == TEXTURE_EXTERNAL) ? "external" :
                                   (e.texture_source == TEXTURE_MODEL ? "model" : "none");
        ej["texture_name"]     = e.texture_name;
        ej["texture_stretch"]  = e.texture_stretch;
        ej["auto_uv"]          = e.auto_uv;
        ej["texture_repeat_u"] = e.texture_repeat_u;
        ej["texture_repeat_v"] = e.texture_repeat_v;
        ej["uv_scale_x"]       = e.uv_scale_vec.x;
        ej["uv_scale_y"]       = e.uv_scale_vec.y;
        ej["segments"]         = e.segments;

        ej["has_light"]        = e.has_light;
        ej["light_color"]      = color_to_hex(e.light.color);
        ej["light_intensity"]  = e.light.intensity;
        ej["light_range"]      = e.light.range;

        ej["asset_name"]       = e.asset_name;

        j["entities"].push_back(ej);
    }

    std::ofstream f(folder_path + "/scene.json");
    if (!f.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open scene.json for writing: %s", folder_path.c_str());
        return;
    }
    f << j.dump(4);
    f.close();
}

bool project_load(const std::string& folder_path, Scene& scene, Shader shader) {
    TraceLog(LOG_INFO, "project_load called: %s", folder_path.c_str());
    std::string json_path = folder_path + "/scene.json";
    if (!fs::exists(json_path)) return false;

    scene.release_resources();
    refresh_textures(nullptr, folder_path);
    refresh_assets(folder_path);
    refresh_models(folder_path, scene);

    std::ifstream f(json_path);
    json j;
    f >> j;

    for (auto& ej : j["entities"]) {
        Entity e;
        e.name = ej["name"].get<std::string>();
        e.type = (ObjectType)ej["type"].get<int>();
        e.id   = static_cast<int>(scene.entities.size());

        e.position = {
            ej["position"][0].get<float>(),
            ej["position"][1].get<float>(),
            ej["position"][2].get<float>()
        };

        e.rotation = {
            ej["rotation"][0].get<float>(),
            ej["rotation"][1].get<float>(),
            ej["rotation"][2].get<float>()
        };

        e.scale = {
            ej["scale"][0].get<float>(),
            ej["scale"][1].get<float>(),
            ej["scale"][2].get<float>()
        };

        e.color         = hex_to_color(ej["color"].get<std::string>());
        e.outline_color = hex_to_color(ej["outline_color"].get<std::string>());

        e.texture_stretch  = ej["texture_stretch"].get<bool>();
        e.auto_uv          = ej["auto_uv"].get<bool>();
        e.texture_repeat_u = ej["texture_repeat_u"].get<float>();
        e.texture_repeat_v = ej["texture_repeat_v"].get<float>();
        e.uv_scale_vec.x   = ej["uv_scale_x"].get<float>();
        e.uv_scale_vec.y   = ej["uv_scale_y"].get<float>();
        e.segments         = ej["segments"].get<int>();

        std::string asset_name = ej["asset_name"].get<std::string>();
        e.asset_name = asset_name;

        for (auto& a : assets) {
            if (a.name == asset_name) {
                e.asset = &a;
                e.type = a.type;

                if (a.is_procedural) {
                    e.model = a.generator(e.segments);
                    e.owns_model_instance = true;
                } 
                
                else {
                    if (!load_model_instance(a, e.model)) {
                        e.asset = nullptr;
                        e.asset_name.clear();
                        e.model = {0};
                        e.owns_model_instance = false;
                        break;
                    }
                    e.owns_model_instance = true;
                }

                store_uv(&e);
                store_material_textures(&e);
                break;
            }
        }

        if (!e.asset || (e.asset_name.size() > 0 && (e.model.meshCount <= 0 || e.model.meshes == nullptr))) {
            continue;
        }

        std::string tex_name = "None";
        if (ej.contains("texture")) {
            tex_name = ej["texture"].get<std::string>();
        }

        std::string texture_source = "none";
        if (ej.contains("texture_source")) {
            texture_source = ej["texture_source"].get<std::string>();
        }

        e.texture = {0};
        e.texture_name.clear();

        if (texture_source == "external") {
            e.texture_source = TEXTURE_EXTERNAL;
            if (ej.contains("texture_name")) {
                e.texture_name = ej["texture_name"].get<std::string>();
            } else {
                e.texture_name = tex_name;
            }

            for (auto& opt : texture_options) {
                if (opt.name == e.texture_name) {
                    e.texture = opt.texture;
                    break;
                }
            }
        } else if (texture_source == "model" || tex_name == "__model__") {
            e.texture_source = TEXTURE_MODEL;
            restore_model_textures(&e);
        } else {
            bool has_embedded = false;
            for (int i = 0; i < e.model.materialCount; i++) {
                if (e.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                    has_embedded = true;
                    break;
                }
            }

            if (tex_name != "None" && tex_name != "__model__") {
                e.texture_source = TEXTURE_EXTERNAL;
                e.texture_name = tex_name;
                for (auto& opt : texture_options) {
                    if (opt.name == e.texture_name) {
                        e.texture = opt.texture;
                        break;
                    }
                }
            } else if (has_embedded) {
                e.texture_source = TEXTURE_MODEL;
                restore_model_textures(&e);
            } else {
                e.texture_source = TEXTURE_NONE;
                clear_material_textures(&e);
            }
        }

        for (int i = 0; i < e.model.materialCount; i++) {
            e.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

            if (e.texture.id != 0) {
                e.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e.texture;
            }
            // If no external texture is selected, keep the model's embedded diffuse texture.

            e.model.materials[i].shader = shader;
        }

        e.shader_assigned = true;

        e.has_light = ej["has_light"].get<bool>();
        if (e.has_light) {
            e.light = create_lighting(e.position, hex_to_color(ej["light_color"].get<std::string>()));
            e.light.intensity = ej["light_intensity"].get<float>();
            e.light.range     = ej["light_range"].get<float>();

            int new_id = allocate_light_id();
            if (new_id != -1) {
                e.light.id = new_id;
                e.light.light = create_light_at_slot(new_id, LIGHT_POINT, e.position, Vector3Zero(), e.light.color, shader);
                e.light_created = true;
            } 
            
            else {
                e.has_light = false;
            }
        }

        scene.entities.push_back(e);
    }

    return true;
}
