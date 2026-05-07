#include "editor_components_ui.h"
#include "editor_utils.h"
#include "editor_ui.h"
#include "editor_viewers.h"
#include "imgui.h"
#include "headers/models.h"
#include "headers/entity.h"
#include "editor/editor_ui.h"
#include "editor/editor.h"
#include <language_manager.h>
#include <filesystem>

#define lang LanguageManager::get()

bool ComponentUIHelper::should_show_component_menu = false;
int ComponentUIHelper::component_to_remove = -1;

void ComponentUIHelper::draw_entity_inspector(Editor& editor, Entity& entity, Shader shader) {
    ImGui::Spacing();
    
    if (entity.get_components()) {
        auto components_manager = entity.get_components();
        size_t component_count = components_manager->get_component_count();
        component_to_remove = -1;

        for (size_t i = 0; i < component_count; ++i) {
            auto comp = components_manager->get_component(i);
            if (!comp) continue;

            std::string tab_label = comp->get_type_name();
            ImGui::PushID(static_cast<int>(i));
            bool open = ImGui::CollapsingHeader(tab_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            
            if (open) {
                ImGui::Spacing();

                bool is_enabled = comp->enabled;
                if (ImGui::Checkbox(lang.word("enabled"), &is_enabled)) {
                    editor.save_state();
                    comp->enabled = is_enabled;
                }

                ImGui::Spacing();

                if (auto transform = std::dynamic_pointer_cast<TransformComponent>(comp)) draw_transform_component(editor, entity, transform.get());
                else if (auto mesh = std::dynamic_pointer_cast<MeshComponent>(comp)) draw_mesh_component(editor, entity, mesh.get());
                else if (auto light = std::dynamic_pointer_cast<LightComponent>(comp)) draw_light_component(editor, entity, light.get(), shader);
                else if (auto material = std::dynamic_pointer_cast<MaterialComponent>(comp)) draw_material_component(editor, entity, material.get());
                else if (auto collision = std::dynamic_pointer_cast<CollisionComponent>(comp)) draw_collision_component(editor, entity, collision.get());

                ImGui::Spacing();

                bool can_remove = (comp->get_type() != COMPONENT_TRANSFORM && comp->get_type() != COMPONENT_MESH);
                if (!can_remove) {
                    ImGui::BeginDisabled();
                }
                
                if (can_remove) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button(lang.word("remove_component"), ImVec2(-1, 0))) {
                        component_to_remove = static_cast<int>(i);
                    }
                    ImGui::PopStyleColor(2);
                }
                
                if (!can_remove) {
                    ImGui::EndDisabled();
                }
            }
            ImGui::PopID();
            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::Spacing();
        if (ImGui::Button(lang.word("add_component"), ImVec2(-1, 0))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            auto components_manager = entity.get_components();

            if (ImGui::MenuItem(lang.word("material"))) {
                editor.save_state();
                bool already_exists = false;
                
                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_MATERIAL) {
                        already_exists = true;
                        break;
                    }
                }

                if (!already_exists) {
                    components_manager->add_component(std::make_shared<MaterialComponent>());
                }

                else { editor.undo(); }
            }

            if (ImGui::MenuItem(lang.word("collision"))) {
                editor.save_state();
                bool already_exists = false;

                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_COLLISION) {
                        already_exists = true;
                        break;
                    }
                }

                if (!already_exists) {
                    components_manager->add_component(std::make_shared<CollisionComponent>());
                }
            }

            if (ImGui::MenuItem(lang.word("light"))) {
                editor.save_state();
                bool already_exists = false;

                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_LIGHT) {
                        already_exists = true;
                        break;
                    }
                }
                if (!already_exists) {
                    auto light = std::make_shared<LightComponent>();
                    if (auto transform = entity.get_transform_component()) {
                        light->light.position = transform->position;
                    }
                    components_manager->add_component(light);
                } else { editor.undo(); }
            }
            ImGui::EndPopup();
        }

        if (component_to_remove != -1) {
            std::shared_ptr<Component> comp = components_manager->get_component(component_to_remove);

            if (std::shared_ptr<LightComponent> light = std::dynamic_pointer_cast<LightComponent>(comp)) {
                if (light->created) {
                    light->light.enabled = false;
                    
                    if (light->light.id != -1) {
                        update_lighting(shader, light->light);
                        free_light_id(light->light.id);
                    }
                }
            }

            components_manager->remove_component(component_to_remove);
            component_to_remove = -1;
        }
    }
}

void ComponentUIHelper::draw_transform_component(Editor& editor, Entity& entity, TransformComponent* transform) {
    if (!transform) return;

    float position[3] = {transform->position.x, transform->position.y, transform->position.z};
    float rotation[3] = {transform->rotation.x, transform->rotation.y, transform->rotation.z};
    float scale[3] = {transform->scale.x, transform->scale.y, transform->scale.z};

    if (ImGui::DragFloat3(lang.word("position"), position, 0.1f)) {
        editor.save_state();
        transform->position = {position[0], position[1], position[2]};
        mark_entity_bounds_dirty(&entity);
    }

    if (ImGui::DragFloat3(lang.word("rotation"), rotation, 1.0f)) {
        editor.save_state();
        transform->rotation = {rotation[0], rotation[1], rotation[2]};
        mark_entity_bounds_dirty(&entity);
    }

    if (ImGui::DragFloat3(lang.word("scale"), scale, 0.1f)) {
        editor.save_state();

        auto count_neg = [](float x, float y, float z) {
            return (x < 0.0f ? 1 : 0) + (y < 0.0f ? 1 : 0) + (z < 0.0f ? 1 : 0);
        };

        bool was_flipped = count_neg(transform->scale.x, transform->scale.y, transform->scale.z) % 2 != 0;
        bool will_flip = count_neg(scale[0], scale[1], scale[2]) % 2 != 0;

        transform->scale = {scale[0], scale[1], scale[2]};
        mark_entity_bounds_dirty(&entity);
        mark_entity_uv_dirty(&entity);

        if (was_flipped != will_flip) {
            update_model(&entity);
        }
    }
}

void ComponentUIHelper::draw_mesh_component(Editor& editor, Entity& entity, MeshComponent* mesh) {
    if (!mesh) return;

    ImGui::Text(lang.word("mesh_config"));
    ImGui::Spacing();

    if (mesh->asset) {
        ImGui::Text("%s: %s", lang.word("asset"), mesh->asset->name.c_str());
    } else {
        ImGui::Text("%s: %s", lang.word("asset"), lang.word("none"));
    }

    const char* object_type_names[] = {
        lang.word("cube"),
        lang.word("sphere"),
        lang.word("cone"),
        lang.word("cylinder"),
        lang.word("hemisphere"),
        lang.word("torus")
    };

    int current_object_type_index = static_cast<int>(mesh->type);
    bool is_procedural_asset = mesh->asset && mesh->asset->is_procedural;

    if (!is_procedural_asset) {
        ImGui::Text(lang.word("mesh_type"), mesh->asset_name.c_str());
        ImGui::BeginDisabled();
    }

    if (ImGui::Combo(lang.word("mesh_type_combo"), &current_object_type_index, object_type_names, IM_ARRAYSIZE(object_type_names))) {
        if (current_object_type_index != static_cast<int>(mesh->type)) {
            editor.save_state();
            ObjectType new_type = static_cast<ObjectType>(current_object_type_index);

            ModelAsset* new_asset = nullptr;
            for (auto& asset_entry : assets) {
                if (asset_entry.is_procedural && asset_entry.type == new_type) {
                    new_asset = &asset_entry;
                    break;
                }
            }

            if (new_asset) {
                mesh->type = new_type;
                mesh->asset = new_asset;
                mesh->asset_name = new_asset->name;
                update_model(&entity);
                mark_entity_bounds_dirty(&entity);
                mark_entity_uv_dirty(&entity);
                store_uv(&entity);
                store_material_textures(&entity);
            }
        }
    }

    if (!is_procedural_asset) {
        ImGui::EndDisabled();
    }

    if (mesh->asset && mesh->asset->is_procedural) {
        if (ImGui::DragInt(lang.word("segments"), &mesh->segments, 1, 3, 100)) {
            editor.save_state();
            update_model(&entity);
            store_uv(&entity);
            store_material_textures(&entity);
            mark_entity_bounds_dirty(&entity);
            mark_entity_uv_dirty(&entity);
        }
    } else {
        ImGui::Text(lang.word("segments_loaded"), mesh->segments);
    }

    ImGui::Checkbox(lang.word("vertex_gizmo"), &g_mesh_edit_state.enabled);
    if (g_mesh_edit_state.enabled) {
        ImGui::Text(lang.word("mesh_index"), g_mesh_edit_state.mesh_index);
        ImGui::Text(lang.word("triangle_index"), g_mesh_edit_state.triangle_index);
        ImGui::Text(lang.word("vertex_corner"), g_mesh_edit_state.vertex_corner);

        if (ImGui::Button(lang.word("reset_mesh"))) {
            editor.save_state();
            reset_mesh_edit_model(entity);
            g_mesh_edit_state.enabled = false;
        }
    }
}

void ComponentUIHelper::draw_material_component(Editor& editor, Entity& entity, MaterialComponent* material) {
    if (!material) return;

    ImGui::Text(lang.word("material_properties"));
    ImGui::Spacing();

    MaterialComponent* mat = entity.get_material_component();
    MeshComponent* mesh = entity.get_mesh_component();

    static std::vector<std::string> available_materials;
    static std::vector<const char*> material_names_cstr;
    static int selected_material_index = -1;
    static bool materials_list_needs_update = true;

    if (materials_list_needs_update) {
        available_materials = get_all_materials_in_project();
        material_names_cstr.clear();
        
        for (const auto& mat_path : available_materials) {
            material_names_cstr.push_back(mat_path.c_str());
        }
        
        selected_material_index = -1;
        if (!mat->texture_name.empty()) {
            for (int i = 0; i < static_cast<int>(available_materials.size()); ++i) {
                if (available_materials[i] == mat->texture_name) {
                    selected_material_index = i;
                    break;
                }
            }
        }
        
        materials_list_needs_update = false;
    }

    ImGui::Text(lang.word("material_file"));
    if (!material_names_cstr.empty()) {
        if (ImGui::Combo("##material_combo", &selected_material_index, material_names_cstr.data(), 
                         static_cast<int>(material_names_cstr.size()))) {
            if (selected_material_index >= 0 && selected_material_index < static_cast<int>(available_materials.size())) {
                const std::string& selected_path = available_materials[selected_material_index];
                if (std::filesystem::exists(selected_path)) {
                    load_material_to_entity(&entity, selected_path);
                    editor.save_state();
                }
            }
        }
    } else {
        ImGui::Text(lang.word("no_materials_found"));
    }

    if (!mat->texture_name.empty() && mat->texture_name.length() > 4 && 
        mat->texture_name.substr(mat->texture_name.length() - 4) == ".mtl") {
        ImGui::TextDisabled("%s: %s", lang.word("current"), mat->texture_name.c_str());
    } else {
        ImGui::TextDisabled("%s: %s", lang.word("current"), lang.word("none"));
    }
}

void ComponentUIHelper::draw_light_component(Editor& editor, Entity& entity, LightComponent* light, Shader shader) {
    if (!light) return;

    ImGui::Text(lang.word("light_properties"));
    ImGui::Spacing();

    bool changed = false;

    changed |= ImGui::DragFloat3(lang.word("position"), (float*)&light->light.position, 0.1f);
    changed |= ImGui::DragFloat3(lang.word("target"), (float*)&light->light.target, 0.1f);

    changed |= ImGui::DragFloat(lang.word("intensity"), &light->light.intensity, 0.1f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(lang.word("range"), &light->light.range, 0.1f, 0.1f, 100.0f);
    changed |= ImGui::DragFloat(lang.word("spot_angle"), &light->light.spot_angle, 1.0f, 5.0f, 180.0f);

    int light_type = light->light.light.type;

    const char* light_types[] = {
        lang.word("directional"),
        lang.word("point"),
        lang.word("spot")
    };

    if (ImGui::Combo(lang.word("light_type"), &light_type, light_types, IM_ARRAYSIZE(light_types))) {
        light->light.light.type = light_type;
        changed = true;
    }

    if (light->enabled != light->light.enabled) {
        changed = true;
    }

    if (changed) {
        editor.save_state();
        light->light.enabled = light->enabled;

        if (light->light.id != -1)
            update_lighting(shader, light->light);
    }
}

void ComponentUIHelper::draw_collision_component(Editor& editor, Entity& entity, CollisionComponent* collision) {
    if (!collision) return;

    ImGui::Text(lang.word("collision_properties"));
    ImGui::Spacing();

    bool changed = false;

    const char* collider_types[] = {
        lang.word("cube"),
        lang.word("sphere"),
        lang.word("capsule"),
        lang.word("mesh")
    };

    int collider_type = static_cast<int>(collision->collider_type);

    if (ImGui::Combo(lang.word("collider_type"), &collider_type, collider_types, IM_ARRAYSIZE(collider_types))) {
        editor.save_state();

        collision->collider_type = static_cast<ColliderType>(collider_type);
        collision->dirty = true;
        changed = true;
    }

    if (ImGui::Checkbox(lang.word("visualize"), &collision->visualize)) {
        editor.save_state();
        changed = true;
    }

    if (ImGui::DragFloat3(lang.word("center"), (float*)&collision->center, 0.1f)) {
        editor.save_state();

        collision->dirty = true;
        changed = true;
    }

    ImGui::Spacing();

    switch (collision->collider_type) {
        case COLLIDER_BOX: {
            if (ImGui::DragFloat3(lang.word("size"), (float*)&collision->size, 0.1f, 0.01f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            break;
        }

        case COLLIDER_SPHERE: {
            if (ImGui::DragFloat3(lang.word("radius"), (float*)&collision->radius, 0.1f, 0.01f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            break;
        }

        case COLLIDER_CAPSULE: {
            if (ImGui::DragFloat3(lang.word("radius"), (float*)&collision->radius, 0.1f, 0.01f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            if (ImGui::DragFloat3(lang.word("height"), (float*)&collision->height, 0.1f, 0.1f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            break;
        }
    }

    if (changed) {
        mark_entity_bounds_dirty(&entity);
    }
}