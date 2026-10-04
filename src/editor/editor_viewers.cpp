#include "editor/editor_viewers.h"
#include "editor/editor_utils.h"
#include "language_manager.h"
#include "tex.h"
#include "entity.h"
#include "imgui.h"
#include "qcImGui.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include "editor/editor_assets.h"
#include "editor/editor_grid.h"

using namespace qc;

#define lang CLanguageManager::Get()

static void ApplyMaterialSettings(CMaterialViewerState& state);
static void RebuildMaterialPreviewMesh(CMaterialViewerState& state);

void CEditorUiState::Unload()
{
    m_Viewport.Unload();
    m_ModelViewer.Unload();
    m_MaterialViewer.Unload();
}

void CModelViewerState::ReleasePreviewModel()
{
    if (m_PreviewModel.meshCount > 0)
    {
        UnloadModel(m_PreviewModel);
    }
    else
    {
        m_PreviewModel = {};
    }
}

void CModelViewerState::Unload()
{
    ReleasePreviewModel();

    if (m_RenderTexture.id != 0)
    {
        UnloadRenderTexture(m_RenderTexture);
        m_RenderTexture = { 0 };
    }
}

void CMaterialViewerState::ReleasePreviewModel()
{
    if (m_PreviewSphere.meshCount <= 0)
    {
        return;
    }

    if (m_PreviewSphere.materialCount > 0 && m_PreviewSphere.materials && m_PreviewSphere.materials[0].maps)
    {
        m_PreviewSphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
    }

    UnloadModel(m_PreviewSphere);
    m_PreviewSphere = {};
}

void CMaterialViewerState::Unload()
{
    ReleasePreviewModel();

    if (m_RenderTexture.id != 0)
    {
        UnloadRenderTexture(m_RenderTexture);
        m_RenderTexture = { 0 };
    }

    m_DiffuseTexture = { 0 };
    m_DiffuseTextureName.clear();
}

bool OpenModelViewerForAsset(CModelViewerState& state, const CModelAsset& asset)
{
    state.ReleasePreviewModel();

    if (!CModelService::LoadInstance(asset, state.m_PreviewModel))
    {
        return false;
    }

    state.m_Visible = true;
    state.m_Orbit.Radius = 5.0f;
    state.m_Orbit.Phi = 20.0f;
    state.m_Orbit.Theta = 45.0f;
    state.m_Orbit.Target = qc::Vec3(0, 0, 0);
    state.m_Orbit.ModelRotation = { 0, 0, 0 };

    const BoundingBox box = GetModelBoundingBox(state.m_PreviewModel);
    state.m_ModelCenter = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    return true;
}

bool OpenMaterialViewerForPath(CEditor& editor, CMaterialViewerState& state, const std::filesystem::path& materialPath)
{
    std::ifstream materialFile(materialPath);
    if (!materialFile.is_open())
    {
        return false;
    }

    state.m_CurrentPath = materialPath;
    state.m_DiffuseTextureName = "";

    state.m_DiffuseTexture = {0};

    state.ReleasePreviewModel();

    state.m_PreviewSphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));
    Material& material = state.m_PreviewSphere.materials[0];
    state.m_Albedo = WHITE;
    state.m_aAlbedo[0] = 1.0f;
    state.m_aAlbedo[1] = 1.0f;
    state.m_aAlbedo[2] = 1.0f;
    state.m_aAlbedo[3] = 1.0f;
    state.m_Brightness = 1.0f;

    std::string line;
    while (std::getline(materialFile, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "Kd")
        {
            float r = 1.0f;
            float g = 1.0f;
            float b = 1.0f;
            if (stream >> r >> g >> b)
            {
                material.maps[MATERIAL_MAP_DIFFUSE].color = {
                    static_cast<unsigned char>(r * 255),
                    static_cast<unsigned char>(g * 255),
                    static_cast<unsigned char>(b * 255),
                    255
                };
                state.m_Albedo = material.maps[MATERIAL_MAP_DIFFUSE].color;
                state.m_aAlbedo[0] = r;
                state.m_aAlbedo[1] = g;
                state.m_aAlbedo[2] = b;
                state.m_aAlbedo[3] = 1.0f;
            }
        }
        else if (type == "map_Kd")
        {
            std::string textureName;
            if (!(stream >> textureName))
            {
                continue;
            }

            state.m_DiffuseTextureName = textureName;
            const std::filesystem::path texturePath = materialPath.parent_path() / textureName;
            const qc::Texture2D* pTexture = editor.m_Textures.Load(texturePath.string());

            if (pTexture)
            {
                material.maps[MATERIAL_MAP_DIFFUSE].texture = *pTexture;
                state.m_DiffuseTexture = *pTexture;
            }
        }
    }

    RebuildMaterialPreviewMesh(state);
    state.m_Visible = true;
    state.m_Orbit.Radius = 2.5f;
    state.m_Orbit.Phi = 20.0f;
    state.m_Orbit.Theta = 45.0f;
    state.m_Orbit.Target = qc::Vec3(0, 0, 0);
    state.m_Orbit.ModelRotation = { 0, 0, 0 };
    return true;
}

void LoadMaterialToEntity(CEntity* pEntity, const std::filesystem::path& mtlPath, int materialSlot)
{
    if (!pEntity || !std::filesystem::exists(mtlPath))
    {
        return;
    }

    std::ifstream materialFile(mtlPath);
    if (!materialFile.is_open())
    {
        return;
    }

    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    CMaterialComponent* pMatComp = pEntity->GetMaterialComponent();
    if (!pMesh || !pMatComp)
    {
        return;
    }
    const auto appliesToSlot = [pMesh, materialSlot](int materialIndex)
    {
        return materialSlot < 0 || materialIndex == materialSlot;
    };

    Color albedo = WHITE;
    std::string textureName;
    std::string normalTextureName;
    std::string roughnessTextureName;
    std::string metallicTextureName;

    std::string line;
    while (std::getline(materialFile, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "Kd")
        {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (stream >> r >> g >> b)
            {
                albedo = {
                    static_cast<unsigned char>(r * 255),
                    static_cast<unsigned char>(g * 255),
                    static_cast<unsigned char>(b * 255),
                    255
                };
            }
        }
        else if (type == "map_Kd")
        {
            if (!(stream >> textureName))
            {
                textureName.clear();
            }
        }
        else if (type == "map_Bump" || type == "bump" || type == "norm")
        {
            if (!(stream >> normalTextureName))
            {
                normalTextureName.clear();
            }
        }
        else if (type == "map_Pr" || type == "map_roughness")
        {
            if (!(stream >> roughnessTextureName))
            {
                roughnessTextureName.clear();
            }
        }
        else if (type == "map_Pm" || type == "map_metallic")
        {
            if (!(stream >> metallicTextureName))
            {
                metallicTextureName.clear();
            }
        }
    }

    materialFile.close();

    pMatComp->m_Color = albedo;
    pMatComp->m_AlbedoTextureName.clear();
    pMatComp->m_NormalTextureName = normalTextureName;
    pMatComp->m_RoughnessTextureName = roughnessTextureName;
    pMatComp->m_MetallicTextureName = metallicTextureName;
    if (pMesh->m_Model.materials)
    {
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (!appliesToSlot(i))
            {
                continue;
            }
            pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = albedo;
        }
    }

    if (!textureName.empty())
    {
        std::filesystem::path texturePath = mtlPath.parent_path() / textureName;
        if (std::filesystem::exists(texturePath))
        {
            Texture2D tex = LoadTexture(texturePath.string().c_str());
            if (tex.id != 0)
            {
                pMatComp->m_Texture = tex;
                pMatComp->m_TextureName = textureName;
                pMatComp->m_TextureSource = TEXTURE_EXTERNAL;

                if (pMesh->m_Model.materials)
                {
                    for (int i = 0; i < pMesh->m_Model.materialCount; i++)
                    {
                        if (!appliesToSlot(i))
                        {
                            continue;
                        }
                        pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
                    }
                }
            }
        }
    }

    const auto loadMap = [&](const std::string& path, int mapType)
    {
        if (path.empty() || !pMesh->m_Model.materials)
        {
            return;
        }
        const std::filesystem::path texturePath = mtlPath.parent_path() / path;
        if (!std::filesystem::exists(texturePath))
        {
            return;
        }
        Texture2D texture = LoadTexture(texturePath.string().c_str());
        if (texture.id == 0)
        {
            return;
        }
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (!appliesToSlot(i))
            {
                continue;
            }
            if (pMesh->m_Model.materials[i].maps)
            {
                pMesh->m_Model.materials[i].maps[mapType].texture = texture;
            }
        }
    };

    loadMap(normalTextureName, MATERIAL_MAP_NORMAL);
    loadMap(roughnessTextureName, MATERIAL_MAP_ROUGHNESS);
    loadMap(metallicTextureName, MATERIAL_MAP_METALNESS);

    pMatComp->m_TextureName = mtlPath.string();
}

std::vector<std::string> GetAllMaterialsInProject()
{
    std::vector<std::string> vMaterials;
    try
    {
        std::filesystem::path currentPath = std::filesystem::current_path();

        for (const auto& entry : std::filesystem::recursive_directory_iterator(currentPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".mtl")
            {
                std::filesystem::path relPath = std::filesystem::relative(entry.path(), currentPath);
                vMaterials.push_back(relPath.generic_string());
            }
        }

        std::sort(vMaterials.begin(), vMaterials.end());
    } catch (const std::exception&)
    {
    }

    return vMaterials;
}

static void ApplyMaterialSettings(CMaterialViewerState& state)
{
    if (state.m_PreviewSphere.meshCount == 0)
    {
        return;
    }

    Material& mat = state.m_PreviewSphere.materials[0];

    Color finalColor = {
        (unsigned char)(state.m_Albedo.r * state.m_Brightness),
        (unsigned char)(state.m_Albedo.g * state.m_Brightness),
        (unsigned char)(state.m_Albedo.b * state.m_Brightness),
        state.m_Albedo.a
    };

    mat.maps[MATERIAL_MAP_DIFFUSE].color = finalColor;

    if (state.m_DiffuseTexture.id != 0)
    {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = state.m_DiffuseTexture;
    }
}

static void RebuildMaterialPreviewMesh(CMaterialViewerState& state)
{
    state.ReleasePreviewModel();

    Mesh mesh = {0};

    switch (state.m_PreviewPrimitive)
    {
        case 0: mesh = GenMeshSphere(1.0f, 64, 64); break;
        case 1: mesh = GenMeshCube(2.0f, 2.0f, 2.0f); break;
        case 2: mesh = GenMeshPlane(3.0f, 3.0f, 1, 1); break;
    }

    state.m_PreviewSphere = LoadModelFromMesh(mesh);
    ApplyMaterialSettings(state);
}

void SaveMaterialToFile(CEditor& editor, CMaterialViewerState& state)
{
    if (state.m_CurrentPath.empty())
    {
        return;
    }

    std::ofstream materialFile(state.m_CurrentPath);
    if (!materialFile.is_open())
    {
        return;
    }

    materialFile << "# Material exported from QuarkEngine\n";
    materialFile << "Kd " << (state.m_aAlbedo[0]) << " " << (state.m_aAlbedo[1]) << " " << (state.m_aAlbedo[2]) << "\n";

    if (!state.m_DiffuseTextureName.empty())
    {
        materialFile << "map_Kd " << state.m_DiffuseTextureName << "\n";
    }

    materialFile.close();
    editor.m_Previews.InvalidateMaterialPreviews();

    for (CEntity& entity : editor.m_Scene.m_vEntities)
    {
        if (!&entity)
        {
            continue;
        }

        CMaterialComponent* pMat = entity.GetMaterialComponent();
        if (!pMat)
        {
            continue;
        }

        if (!pMat->m_TextureName.empty() && std::filesystem::absolute(pMat->m_TextureName) == std::filesystem::absolute(state.m_CurrentPath))
        {
            LoadMaterialToEntity(&entity, state.m_CurrentPath);
        }
    }
}

static void LoadTexturesInDirectory(CMaterialViewerState& state)
{
    state.m_vTextureFilesInDir.clear();
    if (state.m_CurrentPath.empty())
    {
        return;
    }

    const auto materialDir = state.m_CurrentPath.parent_path();
    if (!std::filesystem::exists(materialDir))
    {
        return;
    }

    const std::vector<std::string> vImageExtensions = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds"};

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(materialDir))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (std::find(vImageExtensions.begin(), vImageExtensions.end(), ext) != vImageExtensions.end())
                {
                    state.m_vTextureFilesInDir.push_back(entry.path().filename().string());
                }
            }
        }
    }
    catch (...)
    {
    }
}

static void LoadMaterialTexture(CEditor& editor, CMaterialViewerState& state, const std::string& textureName)
{
    if (state.m_CurrentPath.empty())
    {
        return;
    }

    const auto materialDir = state.m_CurrentPath.parent_path();
    const auto textureFullPath = materialDir / textureName;

    if (!std::filesystem::exists(textureFullPath))
    {
        return;
    }

    const qc::Texture2D* pTexture = editor.m_Textures.Load(textureFullPath.string());
    if (!pTexture)
    {
        return;
    }

    state.m_DiffuseTexture = *pTexture;
    state.m_DiffuseTextureName = textureName;
    ApplyMaterialSettings(state);
}



void DrawModelViewerWindow(CModelViewerState& state)
{
    if (!state.m_Visible)
    {
        state.ReleasePreviewModel();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(lang.Word("model_preview"), &state.m_Visible))
    {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1)
        {
            size.x = 1;
        }
        if (size.y < 1)
        {
            size.y = 1;
        }

        if (state.m_RenderTexture.id == 0 || state.m_RenderTexture.texture.width != (int)size.x || state.m_RenderTexture.texture.height != (int)size.y)
        {
            if (state.m_RenderTexture.id != 0)
            {
                UnloadRenderTexture(state.m_RenderTexture);
            }
            state.m_RenderTexture = LoadRenderTexture((int)size.x, (int)size.y);
        }

        ImVec2 viewportPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("ModelViewport", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool isHovered = ImGui::IsItemHovered();
        bool isActive = ImGui::IsItemActive();

        if (isHovered)
        {
            state.m_Orbit.Radius -= ImGui::GetIO().MouseWheel * 1.5f;
            if (state.m_Orbit.Radius < 0.1f)
            {
                state.m_Orbit.Radius = 0.1f;
            }
        }

        if (isActive)
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                state.m_Orbit.ModelRotation.y += delta.x * 0.5f;
                state.m_Orbit.ModelRotation.x += delta.y * 0.5f;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                state.m_Orbit.Theta -= delta.x * 0.5f;
                state.m_Orbit.Phi -= delta.y * 0.5f;
            }

            if (state.m_Orbit.Phi > 89.0f)
            {
                state.m_Orbit.Phi = 89.0f;
            }
            if (state.m_Orbit.Phi < -89.0f)
            {
                state.m_Orbit.Phi = -89.0f;
            }
        }

        Camera3D cam;
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        cam.target = state.m_Orbit.Target;
        cam.up = { 0, 1, 0 };
        cam.position.x = state.m_Orbit.Target.x + state.m_Orbit.Radius * cosf(state.m_Orbit.Phi * DEG2RAD) * sinf(state.m_Orbit.Theta * DEG2RAD);
        cam.position.y = state.m_Orbit.Target.y + state.m_Orbit.Radius * sinf(state.m_Orbit.Phi * DEG2RAD);
        cam.position.z = state.m_Orbit.Target.z + state.m_Orbit.Radius * cosf(state.m_Orbit.Phi * DEG2RAD) * cosf(state.m_Orbit.Theta * DEG2RAD);

        BeginTextureMode(state.m_RenderTexture);
        ClearBackground({ 40, 40, 45, 255 });
        BeginMode3D(cam);
        if (state.m_PreviewModel.meshCount > 0)
        {
            Mat4 matCenter = Mat4::translation(
                -state.m_ModelCenter.x,
                -state.m_ModelCenter.y,
                -state.m_ModelCenter.z
            );

            Mat4 matRotation =
                Mat4::rotationX(state.m_Orbit.ModelRotation.x * DEG2RAD) *
                Mat4::rotationY(state.m_Orbit.ModelRotation.y * DEG2RAD);

            state.m_PreviewModel.transform = matCenter * matRotation;

            DrawModel(state.m_PreviewModel, { 0, 0, 0 }, 1.0f, WHITE);
            DrawModelWires(state.m_PreviewModel, { 0, 0, 0 }, 1.0f, DARKGRAY);
        }
        CInfiniteGrid::Draw(
            cam,
            state.m_RenderTexture.texture.width,
            state.m_RenderTexture.texture.height,
            1.0f,
            DARKGRAY
        );
        EndMode3D();
        EndTextureMode();

        ImGui::SetCursorScreenPos(viewportPos);
        Rectangle src = { 0, 0, (float)state.m_RenderTexture.texture.width, -(float)state.m_RenderTexture.texture.height };
        QcImGuiImageRect(&state.m_RenderTexture.texture, (int)size.x, (int)size.y, src);
    }
    ImGui::End();
}

void DrawMaterialViewerWindow(CEditor& editor, CMaterialViewerState& state, CEntity* pSelectedEntity)
{
    if (!state.m_Visible)
    {
        state.m_PreviewPrimitive = 0;

        state.ReleasePreviewModel();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(lang.Word("material_editor"), &state.m_Visible))
    {

        ImGui::Columns(2, nullptr, true);

        ImGui::BeginChild("MaterialSettings");

        ImGui::Text("%s", lang.Word("material"));

        if (ImGui::ColorEdit4(lang.Word("albedo"), state.m_aAlbedo))
        {
            state.m_Albedo = {
                (unsigned char)(state.m_aAlbedo[0] * 255),
                (unsigned char)(state.m_aAlbedo[1] * 255),
                (unsigned char)(state.m_aAlbedo[2] * 255),
                (unsigned char)(state.m_aAlbedo[3] * 255)
            };

            ApplyMaterialSettings(state);
        }

        if (ImGui::SliderFloat(lang.Word("brightness"), &state.m_Brightness, 0.1f, 2.0f))
        {
            ApplyMaterialSettings(state);
        }

        ImGui::Separator();

        ImGui::Text("%s", lang.Word("texture"));
        ImGui::Text("%s: %s", lang.Word("current"), state.m_DiffuseTextureName.empty() ? lang.Word("none") : state.m_DiffuseTextureName.c_str());

        if (ImGui::Button(lang.Word("select_texture")))
        {
            LoadTexturesInDirectory(state);
            state.m_TexturePickerVisible = !state.m_TexturePickerVisible;
        }

        ImGui::Separator();

        ImGui::Text("%s", lang.Word("uv_settings"));

        if (ImGui::Checkbox(lang.Word("stretch_texture"), &state.m_TextureStretch))
        {
        }

        if (!state.m_TextureStretch)
        {
            if (ImGui::SliderFloat(lang.Word("repeat_u"), &state.m_TextureRepeatU, 0.1f, 10.0f))
            {
            }

            if (ImGui::SliderFloat(lang.Word("repeat_v"), &state.m_TextureRepeatV, 0.1f, 10.0f))
            {
            }

            if (ImGui::SliderFloat(lang.Word("uv_scale_x"), &state.m_UvScaleX, 0.1f, 5.0f))
            {
            }

            if (ImGui::SliderFloat(lang.Word("uv_scale_y"), &state.m_UvScaleY, 0.1f, 5.0f))
            {
            }
        }

        ImGui::Separator();

        if (ImGui::ColorEdit4(lang.Word("outline_color"), state.m_aOutlineColor))
        {
            state.m_OutlineColor = {
                (unsigned char)(state.m_aOutlineColor[0] * 255),
                (unsigned char)(state.m_aOutlineColor[1] * 255),
                (unsigned char)(state.m_aOutlineColor[2] * 255),
                (unsigned char)(state.m_aOutlineColor[3] * 255)
            };
        }

        ImGui::Separator();

        ImGui::Text("%s", lang.Word("primitive"));

        const char* apPrimitives[] = { lang.Word("sphere"), lang.Word("cube"), lang.Word("plane") };
        if (ImGui::Combo(lang.Word("mesh"), &state.m_PreviewPrimitive, apPrimitives, 3))
        {
            RebuildMaterialPreviewMesh(state);
        }

        ImGui::Separator();

        if (ImGui::Button(lang.Word("save_material"), ImVec2(-1, 0)))
        {
            SaveMaterialToFile(editor, state);
        }

        ImGui::EndChild();

        ImGui::NextColumn();

        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1)
        {
            size.x = 1;
        }
        if (size.y < 1)
        {
            size.y = 1;
        }

        if (state.m_RenderTexture.id == 0 ||
            state.m_RenderTexture.texture.width != (int)size.x ||
            state.m_RenderTexture.texture.height != (int)size.y)
            {

            if (state.m_RenderTexture.id != 0)
            {
                UnloadRenderTexture(state.m_RenderTexture);
            }
            state.m_RenderTexture = LoadRenderTexture((int)size.x, (int)size.y);
        }

        ImVec2 viewportPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("Viewport", size,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        if (ImGui::IsItemHovered())
        {
            state.m_Orbit.Radius -= ImGui::GetIO().MouseWheel * 0.5f;
            if (state.m_Orbit.Radius < 0.1f)
            {
                state.m_Orbit.Radius = 0.1f;
            }
        }

        if (ImGui::IsItemActive())
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                state.m_Orbit.ModelRotation.y += delta.x * 0.5f;
                state.m_Orbit.ModelRotation.x += delta.y * 0.5f;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                state.m_Orbit.Theta -= delta.x * 0.5f;
                state.m_Orbit.Phi -= delta.y * 0.5f;
            }

            state.m_Orbit.Phi = Clamp(state.m_Orbit.Phi, -89.0f, 89.0f);
        }

        Camera3D cam;
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        cam.target = state.m_Orbit.Target;
        cam.up = { 0, 1, 0 };

        cam.position.x = state.m_Orbit.Target.x + state.m_Orbit.Radius * cosf(state.m_Orbit.Phi * DEG2RAD) * sinf(state.m_Orbit.Theta * DEG2RAD);
        cam.position.y = state.m_Orbit.Target.y + state.m_Orbit.Radius * sinf(state.m_Orbit.Phi * DEG2RAD);
        cam.position.z = state.m_Orbit.Target.z + state.m_Orbit.Radius * cosf(state.m_Orbit.Phi * DEG2RAD) * cosf(state.m_Orbit.Theta * DEG2RAD);

        BeginTextureMode(state.m_RenderTexture);
        ClearBackground({ 40, 40, 45, 255 });

        BeginMode3D(cam);

        if (state.m_PreviewSphere.meshCount > 0)
        {
            Mat4 rot =
                Mat4::rotationX(state.m_Orbit.ModelRotation.x * DEG2RAD) *
                Mat4::rotationY(state.m_Orbit.ModelRotation.y * DEG2RAD);

            state.m_PreviewSphere.transform = rot;
            DrawModel(state.m_PreviewSphere, {0,0,0}, 1.0f, WHITE);
        }

        EndMode3D();
        EndTextureMode();

        ImGui::SetCursorScreenPos(viewportPos);

        Rectangle src = {
            0, 0,
            (float)state.m_RenderTexture.texture.width,
            -(float)state.m_RenderTexture.texture.height
        };

        QcImGuiImageRect(&state.m_RenderTexture.texture, (int)size.x, (int)size.y, src);

        ImGui::Columns(1);
    }

    ImGui::End();

    if (state.m_TexturePickerVisible)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(lang.Word("select_texture"), &state.m_TexturePickerVisible))
        {
            ImGui::Text("%s", lang.Word("textures_in_directory"));
            ImGui::Separator();

            for (const auto& textureName : state.m_vTextureFilesInDir)
            {
                if (ImGui::Selectable(textureName.c_str(), state.m_SelectedTextureName == textureName))
                {
                    state.m_SelectedTextureName = textureName;
                    LoadMaterialTexture(editor, state, textureName);
                    state.m_TexturePickerVisible = false;
                }
            }

            if (state.m_vTextureFilesInDir.empty())
            {
                ImGui::TextDisabled("%s", lang.Word("no_textures_found"));
            }

            ImGui::End();
        }
    }
}

