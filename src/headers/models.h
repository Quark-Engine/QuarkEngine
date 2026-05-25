#pragma once
#include "entity.h"
#include <functional>
#include <filesystem>

extern std::vector<ModelAsset> assets;

struct Scene;

void load_models();
void update_model(Entity* e);
void unload_models();
void rebuild_mesh_normals(Mesh& mesh);
void load_external_models(std::string project_path);
void refresh_models(std::string project_path, Scene& scene);
bool ensure_model_asset_loaded(ModelAsset& asset);
bool load_model_instance(const ModelAsset& asset, Model& model);
bool is_model_file(const std::filesystem::path& p);
bool entity_owns_model(const Entity& entity);
void clear_mesh_overrides(Entity& entity);
bool entity_has_mesh_overrides(const Entity& entity);
void capture_mesh_overrides_from_model(Entity& entity);
bool apply_mesh_overrides(Entity& entity);
bool get_mesh_triangle_vertex_indices(const Mesh& mesh, int triangle_index, int out_indices[3]);
bool detach_mesh_triangles(Entity& entity);