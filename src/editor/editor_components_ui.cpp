#include "editor_components_ui.h"
#include "editor_utils.h"
#include "editor_ui.h"
#include "imgui.h"
#include "headers/models.h"
#include "headers/entity.h"
#include "editor/editor_ui.h"
#include "editor/editor.h"

bool ComponentUIHelper::should_show_component_menu = false;
int ComponentUIHelper::component_to_remove = -1;

void ComponentUIHelper::draw_entity_inspector(Editor& editor, Entity& entity) {
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
                if (ImGui::Checkbox("Enabled", &is_enabled)) {
                    editor.save_state();
                    comp->enabled = is_enabled;
                }

                ImGui::Spacing();

                if (auto transform = std::dynamic_pointer_cast<TransformComponent>(comp)) {
                    draw_transform_component(editor, entity, transform.get());
                }
                else if (auto mesh = std::dynamic_pointer_cast<MeshComponent>(comp)) {
                    draw_mesh_component(editor, entity, mesh.get());
                }
                else if (auto light = std::dynamic_pointer_cast<LightComponent>(comp)) {
                    draw_light_component(editor, entity, light.get());
                }

                ImGui::Spacing();

                bool can_remove = (comp->get_type() != COMPONENT_TRANSFORM && comp->get_type() != COMPONENT_MESH);
                if (!can_remove) {
                    ImGui::BeginDisabled();
                }
                
                if (can_remove) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
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
        if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            auto components_manager = entity.get_components();

            if (ImGui::MenuItem("Light")) {
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

    if (ImGui::DragFloat3("Position", position, 0.1f)) {
        editor.save_state();
        transform->position = {position[0], position[1], position[2]};
        mark_entity_bounds_dirty(&entity);
    }

    if (ImGui::DragFloat3("Rotation", rotation, 1.0f)) {
        editor.save_state();
        transform->rotation = {rotation[0], rotation[1], rotation[2]};
        mark_entity_bounds_dirty(&entity);
    }

    if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
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

    ImGui::Text("Mesh Configuration");
    ImGui::Spacing();

    if (mesh->asset) {
        ImGui::Text("Asset: %s", mesh->asset->name.c_str());
    } else {
        ImGui::Text("Asset: None");
    }

    const char* object_type_names[] = { "Cube", "Sphere", "Cone", "Cylinder", "HemiSphere", "Torus" };
    int current_object_type_index = static_cast<int>(mesh->type);
    
    bool is_procedural_asset = mesh->asset && mesh->asset->is_procedural;
    
    if (!is_procedural_asset) {
        ImGui::Text("Mesh Type: %s (Loaded Model)", mesh->asset_name.c_str());
        ImGui::BeginDisabled();
    }

    if (ImGui::Combo("Mesh Type", &current_object_type_index, object_type_names, IM_ARRAYSIZE(object_type_names))) {
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
        if (ImGui::DragInt("Segments", &mesh->segments, 1, 3, 100)) {
            editor.save_state();
            update_model(&entity);
            store_uv(&entity);
            store_material_textures(&entity);
            mark_entity_bounds_dirty(&entity);
            mark_entity_uv_dirty(&entity);
        }
    } else {
        ImGui::Text("Segments: %d (N/A for loaded models)", mesh->segments);
    }

    ImGui::Checkbox("Vertex Gizmo", &g_mesh_edit_state.enabled);
    if (g_mesh_edit_state.enabled) {
        ImGui::Text("Mesh Index: %d", g_mesh_edit_state.mesh_index);
        ImGui::Text("Triangle Index: %d", g_mesh_edit_state.triangle_index);
        ImGui::Text("Vertex Corner: %d", g_mesh_edit_state.vertex_corner);

        if (ImGui::Button("Reset Mesh Overrides")) {
            editor.save_state();
            reset_mesh_edit_model(entity);
            g_mesh_edit_state.enabled = false;
        }
    }
}

void ComponentUIHelper::draw_light_component(Editor& editor, Entity& entity, LightComponent* light) {
    if (!light) return;

    ImGui::Text("Light Properties");
    ImGui::Spacing();

    bool changed = false;
    changed |= ImGui::DragFloat3("Position", (float*)&light->light.position, 0.1f);
    changed |= ImGui::DragFloat3("Target", (float*)&light->light.target, 0.1f);

    changed |= ImGui::DragFloat("Intensity", &light->light.intensity, 0.1f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Range", &light->light.range, 0.1f, 0.1f, 100.0f);
    changed |= ImGui::DragFloat("Spot Angle", &light->light.spot_angle, 1.0f, 5.0f, 180.0f);

    int light_type = light->light.light.type;
    const char* light_types[] = {"Directional", "Point", "Spot"};
    if (ImGui::Combo("Light Type##light", &light_type, light_types, IM_ARRAYSIZE(light_types))) {
        light->light.light.type = light_type;
        changed = true;
    }

    if (changed) {
        editor.save_state();
        if (light->enabled) {
            light->light.enabled = true;
        }
    }
}
