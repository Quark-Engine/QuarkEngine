#include "headers/project.h"
#include "headers/entity.h"
#include "headers/lighting.h"
#include "headers/models.h"
#include "headers/tex.h"
#include "headers/version.h"
#include "editor/editor.h"
#include "raylib.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char* QUARK_PROJECT_EXTENSION = ".quarkproj";

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

static fs::path project_manifest_path_for_root(const fs::path& root_path) {
    const std::string root_name = root_path.filename().string();
    const std::string manifest_name = (root_name.empty() ? std::string("project") : root_name) + QUARK_PROJECT_EXTENSION;
    return root_path / manifest_name;
}

static fs::path find_project_manifest(const fs::path& root_path) {
    std::error_code ec;

    if (!fs::exists(root_path, ec) || !fs::is_directory(root_path, ec))
        return {};

    const fs::path preferred = project_manifest_path_for_root(root_path);
    if (fs::exists(preferred)) return preferred;

    for (const auto& entry : fs::directory_iterator(root_path, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == QUARK_PROJECT_EXTENSION)
            return entry.path();
    }

    return {};
}

static fs::path resolve_project_root_path(const fs::path& input_path) {
    if (input_path.empty()) return {};

    fs::path p = fs::absolute(input_path);

    std::error_code ec;

    if (fs::is_directory(p, ec)) {
        return p;
    }

    if (fs::is_regular_file(p, ec)) {
        if (p.extension() == QUARK_PROJECT_EXTENSION) {
            return p.parent_path();
        }

        return p.parent_path();
    }

    return p;
}

static bool write_project_manifest(const fs::path& root_path) {
    fs::create_directories(root_path);

    json manifest;
    manifest["format"] = "quark-project";
    manifest["version"] = QUARK_ENGINE_VERSION;
    manifest["name"] = root_path.filename().string();
    manifest["scene"] = "scene.json";
    manifest["resources"] = "resources";

    const fs::path manifest_path = project_manifest_path_for_root(root_path);
    std::ofstream manifest_file(manifest_path);
    if (!manifest_file.is_open()) return false;
    manifest_file << manifest.dump(4);
    return true;
}

static fs::path resolve_scene_json_path(const fs::path& input_path) {
    const fs::path root_path = resolve_project_root_path(input_path);
    if (root_path.empty()) return {};

    if (fs::is_regular_file(input_path) && input_path.extension() == QUARK_PROJECT_EXTENSION) {
        std::ifstream manifest_file(input_path);
        if (manifest_file.is_open()) {
            json manifest;
            try {
                manifest_file >> manifest;
                if (manifest.contains("scene") && manifest["scene"].is_string()) {
                    return root_path / manifest["scene"].get<std::string>();
                }
            } catch (...) {}
        }
    }

    const fs::path manifest_path = find_project_manifest(root_path);
    if (!manifest_path.empty()) {
        std::ifstream manifest_file(manifest_path);
        if (manifest_file.is_open()) {
            json manifest;
            try {
                manifest_file >> manifest;
                if (manifest.contains("scene") && manifest["scene"].is_string()) {
                    return root_path / manifest["scene"].get<std::string>();
                }
            } catch (...) {}
        }
    }

    return root_path / "scene.json";
}

std::string project_resolve_root(const std::string& path) {
    const fs::path root_path = resolve_project_root_path(fs::path(path));
    if (root_path.empty()) return {};
    return fs::absolute(root_path).string();
}

bool project_is_valid(const std::string& path) {
    const fs::path input_path(path);
    const fs::path root_path = resolve_project_root_path(input_path);
    if (root_path.empty() || !fs::exists(root_path)) return false;
    return fs::exists(resolve_scene_json_path(input_path));
}

void project_new(const std::string& folder_path, Scene& scene) {
    const fs::path root_path = resolve_project_root_path(fs::path(folder_path));
    fs::create_directories(root_path);
    fs::create_directories(root_path / "resources");

    json j;
    j["entities"] = json::array();
    
    std::ofstream f(root_path / "scene.json");
    f << j.dump(4);
    write_project_manifest(root_path);

    scene.release_resources();
}

void project_save(const std::string& folder_path, const Scene& scene) {
    const fs::path root_path = resolve_project_root_path(fs::path(folder_path));
    fs::create_directories(root_path / "resources");

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
        ej["mesh_triangles_detached"] = e.mesh_triangles_detached;
        ej["mesh_vertex_overrides"] = json::array();
        for (const auto& mesh_vertices : e.mesh_vertex_overrides) {
            ej["mesh_vertex_overrides"].push_back(mesh_vertices);
        }

        ej["has_light"]        = e.has_light;
        ej["light_color"]      = color_to_hex(e.light.color);
        ej["light_intensity"]  = e.light.intensity;
        ej["light_range"]      = e.light.range;
        ej["light_spot_angle"] = e.light.spot_angle;
        ej["light_target"]     = json::array({ (float)e.light.target.x, (float)e.light.target.y, (float)e.light.target.z });
        ej["light_rotation"]   = json::array({ (float)e.light.rotation.x, (float)e.light.rotation.y, (float)e.light.rotation.z });

        ej["asset_name"]       = e.asset_name;

        j["entities"].push_back(ej);
    }

    std::ofstream f(root_path / "scene.json");
    if (!f.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open scene.json for writing: %s", folder_path.c_str());
        return;
    }
    f << j.dump(4);
    f.close();
    write_project_manifest(root_path);
}

bool project_load(const std::string& folder_path, Scene& scene, Shader shader) {
    TraceLog(LOG_INFO, "project_load called: %s", folder_path.c_str());
    const fs::path root_path = resolve_project_root_path(fs::path(folder_path));
    const fs::path json_path = resolve_scene_json_path(fs::path(folder_path));
    if (!fs::exists(json_path)) return false;

    scene.release_resources();
    refresh_textures(nullptr, root_path.string());
    refresh_assets(root_path.string());
    refresh_models(root_path.string(), scene);

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
        e.mesh_triangles_detached = ej.value("mesh_triangles_detached", false);

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
                if (ej.contains("mesh_vertex_overrides") && ej["mesh_vertex_overrides"].is_array()) {
                    e.mesh_vertex_overrides.clear();
                    for (const auto& mesh_json : ej["mesh_vertex_overrides"]) {
                        if (!mesh_json.is_array()) {
                            e.mesh_vertex_overrides.emplace_back();
                            continue;
                        }

                        std::vector<float> vertices;
                        vertices.reserve(mesh_json.size());
                        for (const auto& value : mesh_json) {
                            vertices.push_back(value.get<float>());
                        }
                        e.mesh_vertex_overrides.push_back(std::move(vertices));
                    }
                }
                apply_mesh_overrides(e);
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
            e.light.intensity  = ej["light_intensity"].get<float>();
            e.light.range      = ej["light_range"].get<float>();
            e.light.spot_angle = ej["light_spot_angle"].get<float>();
            e.light.target     = { 
                ej["light_target"][0].get<float>(), 
                ej["light_target"][1].get<float>(), 
                ej["light_target"][2].get<float>() };
            e.light.rotation   = { 
                ej["light_rotation"][0].get<float>(), 
                ej["light_rotation"][1].get<float>(), 
                ej["light_rotation"][2].get<float>() };

            int new_id = allocate_light_id();
            if (new_id != -1) {
                e.light.id = new_id;
                e.light.light = create_light_at_slot(new_id, LIGHT_POINT, e.position, Vector3Zero(), e.light.color, shader);
                initialize_lighting_uniform_cache(e.light, shader, new_id);
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

std::string get_project_version(const std::string& path) {
    const fs::path root_path = resolve_project_root_path(fs::path(path));
    const fs::path manifest_path = find_project_manifest(root_path);
    if (manifest_path.empty()) return "";

    std::ifstream f(manifest_path);
    if (!f.is_open()) return "";

    json manifest;
    try {
        f >> manifest;
        if (manifest.contains("version") && manifest["version"].is_string())
            return manifest["version"].get<std::string>();   
    }
    
    catch (...) {}
    return "";
}