#include "headers/models.h"
#include "headers/scene.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#if defined(_MSC_VER)
#include <excpt.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <csetjmp>
#include <cstdlib>
#endif

std::vector<ModelAsset> assets;

static std::string lowercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

#if defined(__unix__) || defined(__APPLE__)
static sigjmp_buf g_model_load_env;
static volatile sig_atomic_t g_model_load_guard_active = 0;
static struct sigaction g_prev_sigsegv = {};
static struct sigaction g_prev_sigbus = {};
static struct sigaction g_prev_sigabrt = {};

static void model_load_signal_handler(int signal_code) {
    if (g_model_load_guard_active) siglongjmp(g_model_load_env, signal_code);
    std::_Exit(128 + signal_code);
}

static void install_model_load_signal_guards() {
    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_handler = model_load_signal_handler;
    action.sa_flags = 0;

    sigaction(SIGSEGV, &action, &g_prev_sigsegv);
#ifdef SIGBUS
    sigaction(SIGBUS, &action, &g_prev_sigbus);
#endif
    sigaction(SIGABRT, &action, &g_prev_sigabrt);
    g_model_load_guard_active = 1;
}

static void restore_model_load_signal_guards() {
    g_model_load_guard_active = 0;
    sigaction(SIGSEGV, &g_prev_sigsegv, nullptr);
#ifdef SIGBUS
    sigaction(SIGBUS, &g_prev_sigbus, nullptr);
#endif
    sigaction(SIGABRT, &g_prev_sigabrt, nullptr);
}
#endif

struct ObjVertexKey {
    int v = -1;
    int vt = -1;
    int vn = -1;
};

struct ObjTriangle {
    ObjVertexKey a;
    ObjVertexKey b;
    ObjVertexKey c;
};

static bool parse_obj_vertex_token(const std::string& token, ObjVertexKey& out) {
    std::string parts[3];
    int part_index = 0;
    size_t start = 0;

    for (size_t i = 0; i <= token.size() && part_index < 3; i++) {
        if (i == token.size() || token[i] == '/') {
            parts[part_index++] = token.substr(start, i - start);
            start = i + 1;
        }
    }

    auto parse_part = [](const std::string& value, int& result) -> bool {
        if (value.empty()) return true;
        try {
            result = std::stoi(value);
            return true;
        } catch (...) {
            return false;
        }
    };

    out = {};
    return parse_part(parts[0], out.v) && parse_part(parts[1], out.vt) && parse_part(parts[2], out.vn);
}

static int resolve_obj_index(int idx, int count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}

static Vector3 safe_normalize(Vector3 value) {
    const float length = sqrtf(value.x*value.x + value.y*value.y + value.z*value.z);
    if (length <= 0.000001f) return { 0.0f, 1.0f, 0.0f };
    return { value.x / length, value.y / length, value.z / length };
}

bool get_mesh_triangle_vertex_indices(const Mesh& mesh, int triangle_index, int out_indices[3]) {
    if (!out_indices || triangle_index < 0 || triangle_index >= mesh.triangleCount) return false;

    if (mesh.indices) {
        const int base = triangle_index * 3;
        out_indices[0] = static_cast<int>(mesh.indices[base + 0]);
        out_indices[1] = static_cast<int>(mesh.indices[base + 1]);
        out_indices[2] = static_cast<int>(mesh.indices[base + 2]);
    } else {
        const int base = triangle_index * 3;
        out_indices[0] = base + 0;
        out_indices[1] = base + 1;
        out_indices[2] = base + 2;
    }

    for (int i = 0; i < 3; i++) {
        if (out_indices[i] < 0 || out_indices[i] >= mesh.vertexCount) return false;
    }

    return true;
}

static void rebuild_mesh_normals(Mesh& mesh) {
    if (!mesh.vertices || !mesh.normals || mesh.vertexCount <= 0) return;

    for (int i = 0; i < mesh.vertexCount * 3; i++) {
        mesh.normals[i] = 0.0f;
    }

    for (int triangle_index = 0; triangle_index < mesh.triangleCount; triangle_index++) {
        int indices[3] = {};
        if (!get_mesh_triangle_vertex_indices(mesh, triangle_index, indices)) continue;

        const Vector3 a = {
            mesh.vertices[indices[0] * 3 + 0],
            mesh.vertices[indices[0] * 3 + 1],
            mesh.vertices[indices[0] * 3 + 2]
        };
        const Vector3 b = {
            mesh.vertices[indices[1] * 3 + 0],
            mesh.vertices[indices[1] * 3 + 1],
            mesh.vertices[indices[1] * 3 + 2]
        };
        const Vector3 c = {
            mesh.vertices[indices[2] * 3 + 0],
            mesh.vertices[indices[2] * 3 + 1],
            mesh.vertices[indices[2] * 3 + 2]
        };

        const Vector3 ab = Vector3Subtract(b, a);
        const Vector3 ac = Vector3Subtract(c, a);
        const Vector3 normal = safe_normalize(Vector3CrossProduct(ab, ac));

        for (int i = 0; i < 3; i++) {
            mesh.normals[indices[i] * 3 + 0] += normal.x;
            mesh.normals[indices[i] * 3 + 1] += normal.y;
            mesh.normals[indices[i] * 3 + 2] += normal.z;
        }
    }

    for (int vertex_index = 0; vertex_index < mesh.vertexCount; vertex_index++) {
        const Vector3 accumulated = {
            mesh.normals[vertex_index * 3 + 0],
            mesh.normals[vertex_index * 3 + 1],
            mesh.normals[vertex_index * 3 + 2]
        };
        const Vector3 normalized = safe_normalize(accumulated);
        mesh.normals[vertex_index * 3 + 0] = normalized.x;
        mesh.normals[vertex_index * 3 + 1] = normalized.y;
        mesh.normals[vertex_index * 3 + 2] = normalized.z;
    }
}

void clear_mesh_overrides(Entity& entity) {
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return;
    mesh->mesh_vertex_overrides.clear();
    mesh->mesh_triangles_detached = false;
}

bool entity_has_mesh_overrides(const Entity& entity) {
    const MeshComponent* mesh = entity.get_mesh_component();
    return mesh && !mesh->mesh_vertex_overrides.empty();
}

void capture_mesh_overrides_from_model(Entity& entity) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component) return;

    const bool triangles_detached = mesh_component->mesh_triangles_detached;
    mesh_component->mesh_vertex_overrides.clear();
    mesh_component->mesh_triangles_detached = triangles_detached;
    if (mesh_component->model.meshCount <= 0 || !mesh_component->model.meshes) return;

    mesh_component->mesh_vertex_overrides.reserve(mesh_component->model.meshCount);
    for (int mesh_index = 0; mesh_index < mesh_component->model.meshCount; mesh_index++) {
        const Mesh& mesh = mesh_component->model.meshes[mesh_index];
        if (!mesh.vertices || mesh.vertexCount <= 0) {
            mesh_component->mesh_vertex_overrides.emplace_back();
            continue;
        }

        const float* begin = mesh.vertices;
        const float* end = mesh.vertices + mesh.vertexCount * 3;
        mesh_component->mesh_vertex_overrides.emplace_back(begin, end);
    }
}

bool apply_mesh_overrides(Entity& entity) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component) return false;
    if (!entity_has_mesh_overrides(entity)) return false;
    if (mesh_component->model.meshCount <= 0 || !mesh_component->model.meshes) return false;

    if (mesh_component->mesh_triangles_detached) {
        detach_mesh_triangles(entity);
    }

    bool applied_any = false;

    for (int mesh_index = 0; mesh_index < mesh_component->model.meshCount; mesh_index++) {
        if (mesh_index >= static_cast<int>(mesh_component->mesh_vertex_overrides.size())) break;

        Mesh& mesh = mesh_component->model.meshes[mesh_index];
        std::vector<float>& override_vertices = mesh_component->mesh_vertex_overrides[mesh_index];
        if (!mesh.vertices || mesh.vertexCount <= 0) continue;
        if (override_vertices.size() != static_cast<size_t>(mesh.vertexCount * 3)) continue;

        memcpy(mesh.vertices, override_vertices.data(), override_vertices.size() * sizeof(float));
        UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);

        if (mesh.normals) {
            rebuild_mesh_normals(mesh);
            UpdateMeshBuffer(mesh, 2, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);
        }

        applied_any = true;
    }

    return applied_any;
}

static bool detach_single_mesh_triangles(Mesh& mesh) {
    if (!mesh.vertices || mesh.vertexCount <= 0 || mesh.triangleCount <= 0) return false;
    if (!mesh.indices) return true;

    const int detached_vertex_count = mesh.triangleCount * 3;
    std::vector<float> vertices(detached_vertex_count * 3);
    std::vector<float> texcoords;
    std::vector<float> normals;
    std::vector<unsigned char> colors;

    if (mesh.texcoords) texcoords.resize(detached_vertex_count * 2);
    if (mesh.normals) normals.resize(detached_vertex_count * 3);
    if (mesh.colors) colors.resize(detached_vertex_count * 4);

    for (int triangle_index = 0; triangle_index < mesh.triangleCount; triangle_index++) {
        int source_indices[3] = {};
        if (!get_mesh_triangle_vertex_indices(mesh, triangle_index, source_indices)) return false;

        for (int corner = 0; corner < 3; corner++) {
            const int src = source_indices[corner];
            const int dst = triangle_index * 3 + corner;

            vertices[dst * 3 + 0] = mesh.vertices[src * 3 + 0];
            vertices[dst * 3 + 1] = mesh.vertices[src * 3 + 1];
            vertices[dst * 3 + 2] = mesh.vertices[src * 3 + 2];

            if (mesh.texcoords) {
                texcoords[dst * 2 + 0] = mesh.texcoords[src * 2 + 0];
                texcoords[dst * 2 + 1] = mesh.texcoords[src * 2 + 1];
            }

            if (mesh.normals) {
                normals[dst * 3 + 0] = mesh.normals[src * 3 + 0];
                normals[dst * 3 + 1] = mesh.normals[src * 3 + 1];
                normals[dst * 3 + 2] = mesh.normals[src * 3 + 2];
            }

            if (mesh.colors) {
                colors[dst * 4 + 0] = mesh.colors[src * 4 + 0];
                colors[dst * 4 + 1] = mesh.colors[src * 4 + 1];
                colors[dst * 4 + 2] = mesh.colors[src * 4 + 2];
                colors[dst * 4 + 3] = mesh.colors[src * 4 + 3];
            }
        }
    }

    Mesh detached = mesh;
    detached.vertexCount = detached_vertex_count;
    detached.vertices = static_cast<float*>(MemAlloc(vertices.size() * sizeof(float)));
    detached.texcoords = mesh.texcoords ? static_cast<float*>(MemAlloc(texcoords.size() * sizeof(float))) : nullptr;
    detached.normals = mesh.normals ? static_cast<float*>(MemAlloc(normals.size() * sizeof(float))) : nullptr;
    detached.colors = mesh.colors ? static_cast<unsigned char*>(MemAlloc(colors.size() * sizeof(unsigned char))) : nullptr;
    detached.indices = nullptr;
    detached.animVertices = nullptr;
    detached.animNormals = nullptr;
    detached.boneIds = nullptr;
    detached.boneWeights = nullptr;
    detached.boneMatrices = nullptr;
    detached.boneCount = 0;
    detached.vaoId = 0;
    detached.vboId = nullptr;

    if (!detached.vertices ||
        (mesh.texcoords && !detached.texcoords) ||
        (mesh.normals && !detached.normals) ||
        (mesh.colors && !detached.colors)) {
        if (detached.vertices) MemFree(detached.vertices);
        if (detached.texcoords) MemFree(detached.texcoords);
        if (detached.normals) MemFree(detached.normals);
        if (detached.colors) MemFree(detached.colors);
        return false;
    }

    memcpy(detached.vertices, vertices.data(), vertices.size() * sizeof(float));
    if (detached.texcoords) memcpy(detached.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
    if (detached.normals) memcpy(detached.normals, normals.data(), normals.size() * sizeof(float));
    if (detached.colors) memcpy(detached.colors, colors.data(), colors.size() * sizeof(unsigned char));

    rebuild_mesh_normals(detached);
    UploadMesh(&detached, false);
    UnloadMesh(mesh);
    mesh = detached;
    return true;
}

bool detach_mesh_triangles(Entity& entity) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || mesh_component->model.meshCount <= 0 || !mesh_component->model.meshes) return false;

    bool detached_any = false;
    for (int mesh_index = 0; mesh_index < mesh_component->model.meshCount; mesh_index++) {
        Mesh& mesh = mesh_component->model.meshes[mesh_index];
        if (!mesh.indices) continue;
        if (!detach_single_mesh_triangles(mesh)) continue;
        detached_any = true;
    }

    if (detached_any) {
        mesh_component->mesh_triangles_detached = true;
    } else if (!mesh_component->mesh_triangles_detached) {
        mesh_component->mesh_triangles_detached = true;
    }

    return detached_any;
}

static bool append_obj_vertex(
    const ObjVertexKey& key,
    const std::vector<Vector3>& generated_normals,
    const std::vector<Vector3>& positions,
    const std::vector<Vector2>& texcoords,
    const std::vector<Vector3>& normals,
    std::vector<float>& out_vertices,
    std::vector<float>& out_texcoords,
    std::vector<float>& out_normals
) {
    const int v_index = resolve_obj_index(key.v, static_cast<int>(positions.size()));
    if (v_index < 0 || v_index >= static_cast<int>(positions.size())) return false;

    const Vector3& position = positions[v_index];
    out_vertices.push_back(position.x);
    out_vertices.push_back(position.y);
    out_vertices.push_back(position.z);

    Vector2 uv = { 0.0f, 0.0f };
    const int vt_index = resolve_obj_index(key.vt, static_cast<int>(texcoords.size()));
    if (vt_index >= 0 && vt_index < static_cast<int>(texcoords.size())) {
        uv = texcoords[vt_index];
    }
    out_texcoords.push_back(uv.x);
    out_texcoords.push_back(uv.y);

    Vector3 normal = { 0.0f, 1.0f, 0.0f };
    const int vn_index = resolve_obj_index(key.vn, static_cast<int>(normals.size()));
    if (vn_index >= 0 && vn_index < static_cast<int>(normals.size())) {
        normal = normals[vn_index];
    } else if (v_index < static_cast<int>(generated_normals.size())) {
        normal = generated_normals[v_index];
    }
    out_normals.push_back(normal.x);
    out_normals.push_back(normal.y);
    out_normals.push_back(normal.z);

    return true;
}

static Model load_obj_model_fallback(const std::string& filepath, bool& ok) {
    ok = false;

    std::ifstream input(filepath);
    if (!input.is_open()) {
        TraceLog(LOG_WARNING, "Failed to open OBJ file: %s", filepath.c_str());
        return {0};
    }

    std::vector<Vector3> positions;
    std::vector<Vector2> texcoords;
    std::vector<Vector3> normals;
    std::vector<Vector3> generated_normals;
    std::vector<ObjTriangle> triangles;
    std::vector<float> out_vertices;
    std::vector<float> out_texcoords;
    std::vector<float> out_normals;

    std::string line;
    std::string mtl_filename;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t first_non_space = line.find_first_not_of(" \t\r\n");
        if (first_non_space == std::string::npos) continue;

        std::istringstream iss(line.substr(first_non_space));
        std::string type;
        iss >> type;

        if (type == "v") {
            Vector3 v = {0};
            iss >> v.x >> v.y >> v.z;
            positions.push_back(v);
            generated_normals.push_back({ 0.0f, 0.0f, 0.0f });
        } else if (type == "vt") {
            Vector2 vt = {0};
            iss >> vt.x >> vt.y;
            texcoords.push_back(vt);
        } else if (type == "vn") {
            Vector3 vn = {0};
            iss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        } else if (type == "mtllib") {
            iss >> mtl_filename;
        } else if (type == "f") {
            std::vector<ObjVertexKey> face;
            std::string token;
            while (iss >> token) {
                ObjVertexKey key;
                if (!parse_obj_vertex_token(token, key)) {
                    face.clear();
                    break;
                }
                face.push_back(key);
            }

            if (face.size() < 3) continue;

            for (size_t i = 1; i + 1 < face.size(); i++) {
                const int ia = resolve_obj_index(face[0].v, static_cast<int>(positions.size()));
                const int ib = resolve_obj_index(face[i].v, static_cast<int>(positions.size()));
                const int ic = resolve_obj_index(face[i + 1].v, static_cast<int>(positions.size()));
                if (ia < 0 || ib < 0 || ic < 0 ||
                    ia >= static_cast<int>(positions.size()) ||
                    ib >= static_cast<int>(positions.size()) ||
                    ic >= static_cast<int>(positions.size())) {
                    continue;
                }

                const Vector3& a = positions[ia];
                const Vector3& b = positions[ib];
                const Vector3& c = positions[ic];
                const Vector3 ab = { b.x - a.x, b.y - a.y, b.z - a.z };
                const Vector3 ac = { c.x - a.x, c.y - a.y, c.z - a.z };
                const Vector3 face_normal = safe_normalize(Vector3CrossProduct(ab, ac));

                if (face[0].vn == -1) {
                    generated_normals[ia] = Vector3Add(generated_normals[ia], face_normal);
                }
                if (face[i].vn == -1) {
                    generated_normals[ib] = Vector3Add(generated_normals[ib], face_normal);
                }
                if (face[i + 1].vn == -1) {
                    generated_normals[ic] = Vector3Add(generated_normals[ic], face_normal);
                }

                triangles.push_back({ face[0], face[i], face[i + 1] });
            }
        }
    }

    for (Vector3& normal : generated_normals) {
        normal = safe_normalize(normal);
    }

    for (const ObjTriangle& triangle : triangles) {
        if (!append_obj_vertex(triangle.a, generated_normals, positions, texcoords, normals, out_vertices, out_texcoords, out_normals)) continue;
        if (!append_obj_vertex(triangle.b, generated_normals, positions, texcoords, normals, out_vertices, out_texcoords, out_normals)) continue;
        if (!append_obj_vertex(triangle.c, generated_normals, positions, texcoords, normals, out_vertices, out_texcoords, out_normals)) continue;
    }

    if (out_vertices.empty()) {
        TraceLog(LOG_WARNING, "OBJ parser produced no triangles: %s", filepath.c_str());
        return {0};
    }

    Mesh mesh = {0};
    mesh.vertexCount = static_cast<int>(out_vertices.size() / 3);
    mesh.triangleCount = mesh.vertexCount / 3;

    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(out_vertices.size()) * sizeof(float)));
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(out_texcoords.size()) * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(out_normals.size()) * sizeof(float)));

    if (!mesh.vertices || !mesh.texcoords || !mesh.normals) {
        if (mesh.vertices) MemFree(mesh.vertices);
        if (mesh.texcoords) MemFree(mesh.texcoords);
        if (mesh.normals) MemFree(mesh.normals);
        TraceLog(LOG_WARNING, "Failed to allocate mesh buffers for OBJ: %s", filepath.c_str());
        return {0};
    }

    memcpy(mesh.vertices, out_vertices.data(), out_vertices.size() * sizeof(float));
    memcpy(mesh.texcoords, out_texcoords.data(), out_texcoords.size() * sizeof(float));
    memcpy(mesh.normals, out_normals.data(), out_normals.size() * sizeof(float));

    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    ok = (model.meshCount > 0 && model.meshes != nullptr);

    if (ok) {
        if (model.materialCount == 0) {
            model.materials = static_cast<Material*>(MemAlloc(sizeof(Material)));
            model.materials[0] = LoadMaterialDefault();
            model.materialCount = 1;
        }

        if (!mtl_filename.empty()) {
            std::filesystem::path obj_path(filepath);
            std::filesystem::path mtl_path = obj_path.parent_path() / mtl_filename;

            std::ifstream mtl_input(mtl_path);
            if (mtl_input.is_open()) {
                TraceLog(LOG_INFO, "Loading MTL file: %s", mtl_path.string().c_str());
                
                Color diffuse_color = WHITE;
                Color ambient_color = WHITE;
                Color specular_color = WHITE;
                std::string texture_filename;
                float shininess = 32.0f;

                std::string mtl_line;
                while (std::getline(mtl_input, mtl_line)) {
                    if (mtl_line.empty() || mtl_line[0] == '#') continue;

                    size_t m_first = mtl_line.find_first_not_of(" \t\r\n");
                    if (m_first == std::string::npos) continue;

                    std::istringstream mtl_iss(mtl_line.substr(m_first));
                    std::string mtl_type;
                    mtl_iss >> mtl_type;

                    if (mtl_type == "Kd") {
                        float r = 1.0f, g = 1.0f, b = 1.0f;
                        mtl_iss >> r >> g >> b;
                        diffuse_color = {
                            static_cast<unsigned char>(r * 255),
                            static_cast<unsigned char>(g * 255),
                            static_cast<unsigned char>(b * 255),
                            255
                        };
                    } 
                    else if (mtl_type == "Ka") {
                        float r = 1.0f, g = 1.0f, b = 1.0f;
                        mtl_iss >> r >> g >> b;
                        ambient_color = {
                            static_cast<unsigned char>(r * 255),
                            static_cast<unsigned char>(g * 255),
                            static_cast<unsigned char>(b * 255),
                            255
                        };
                    }
                    else if (mtl_type == "Ks") {
                        float r = 1.0f, g = 1.0f, b = 1.0f;
                        mtl_iss >> r >> g >> b;
                        specular_color = {
                            static_cast<unsigned char>(r * 255),
                            static_cast<unsigned char>(g * 255),
                            static_cast<unsigned char>(b * 255),
                            255
                        };
                    }
                    else if (mtl_type == "Ns") {
                        mtl_iss >> shininess;
                    }
                    else if (mtl_type == "map_Kd") {
                        mtl_iss >> texture_filename;
                    }
                }

                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = diffuse_color;

                if (!texture_filename.empty()) {
                    std::filesystem::path tex_path = obj_path.parent_path() / texture_filename;
                    if (std::filesystem::exists(tex_path)) {
                        Texture2D loaded_tex = LoadTexture(tex_path.string().c_str());
                        if (loaded_tex.id > 0) {
                            model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = loaded_tex;
                            TraceLog(LOG_INFO, "Loaded texture for OBJ: %s", tex_path.string().c_str());
                        } else {
                            TraceLog(LOG_WARNING, "Failed to load texture: %s", tex_path.string().c_str());
                        }
                    } else {
                        TraceLog(LOG_WARNING, "Texture file not found: %s", tex_path.string().c_str());
                    }
                }

                TraceLog(LOG_INFO, "Loaded MTL material: Kd=(%d,%d,%d), has_texture=%s", 
                    diffuse_color.r, diffuse_color.g, diffuse_color.b,
                    !texture_filename.empty() ? "yes" : "no");
            } else {
                TraceLog(LOG_WARNING, "Failed to open MTL file: %s", mtl_path.string().c_str());
            }
        }
        
        TraceLog(LOG_INFO, "Loaded OBJ model via fallback parser: %s", filepath.c_str());
    } else {
        TraceLog(LOG_WARNING, "Failed to build OBJ model: %s", filepath.c_str());
        return {0};
    }
    return model;
}

#if defined(_MSC_VER)
static Model try_load_model_native(const std::string& filepath, bool& ok) {
    ok = false;
    Model model = {0};

    __try {
        model = LoadModel(filepath.c_str());
        ok = (model.meshCount > 0 && model.meshes != nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        TraceLog(LOG_ERROR, "Model loader crashed while reading: %s", filepath.c_str());
        model = {0};
        ok = false;
    }

    if (!ok) {
        TraceLog(LOG_WARNING, "Failed to load model: %s", filepath.c_str());
        model = {0};
    }

    return model;
}
#elif defined(__unix__) || defined(__APPLE__)
static Model try_load_model_native(const std::string& filepath, bool& ok) {
    ok = false;
    Model model = {0};

    install_model_load_signal_guards();
    const int signal_code = sigsetjmp(g_model_load_env, 1);
    if (signal_code == 0) {
        model = LoadModel(filepath.c_str());
        ok = (model.meshCount > 0 && model.meshes != nullptr);
    } else {
        TraceLog(LOG_ERROR, "Model loader crashed with signal %d while reading: %s", signal_code, filepath.c_str());
        model = {0};
        ok = false;
    }
    restore_model_load_signal_guards();

    if (!ok) {
        TraceLog(LOG_WARNING, "Failed to load model: %s", filepath.c_str());
        model = {0};
    }

    return model;
}
#else
static Model try_load_model_native(const std::string& filepath, bool& ok) {
    Model model = LoadModel(filepath.c_str());
    ok = (model.meshCount > 0 && model.meshes != nullptr);

    if (!ok) {
        TraceLog(LOG_WARNING, "Failed to load model: %s", filepath.c_str());
        model = {0};
    }

    return model;
}
#endif

static Model try_load_model(const std::string& filepath, bool& ok) {
    const std::string ext = lowercase_copy(std::filesystem::path(filepath).extension().string());

    if (ext == ".obj") {
        Model model = load_obj_model_fallback(filepath, ok);
        if (ok) return model;
    }

    return try_load_model_native(filepath, ok);
}

bool ensure_model_asset_loaded(ModelAsset& asset) {
    if (asset.is_procedural) return true;
    if (asset.loaded_model.meshCount > 0 && asset.loaded_model.meshes != nullptr) return true;
    if (asset.filepath.empty()) return false;

    bool loaded = false;
    asset.loaded_model = try_load_model(asset.filepath, loaded);
    return loaded;
}

bool load_model_instance(const ModelAsset& asset, Model& model) {
    model = {0};

    if (asset.is_procedural) {
        if (!asset.generator) return false;
        model = asset.generator(16);
        return model.meshCount > 0 && model.meshes != nullptr;
    }

    if (asset.filepath.empty()) return false;

    bool loaded = false;
    model = try_load_model(asset.filepath, loaded);
    return loaded;
}

static std::vector<std::filesystem::path> collect_model_paths(const std::filesystem::path& resource_dir) {
    namespace fs = std::filesystem;

    std::vector<fs::path> result;
    std::error_code ec;
    fs::recursive_directory_iterator it(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return result;

    for (const auto& entry : it) {
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }

        if (is_model_file(entry.path())) {
            result.push_back(entry.path());
        }
    }

    return result;
}

bool is_model_file(const std::filesystem::path& p) {
    std::string ext = lowercase_copy(p.extension().string());

    return ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".iqm";
}

bool entity_owns_model(const Entity& entity) {
    const MeshComponent* mesh = entity.get_mesh_component();
    return mesh && mesh->owns_model_instance;
}

void load_models() {
    ModelAsset cube_asset;
    cube_asset.name = "Cube";
    cube_asset.type = CUBE;
    cube_asset.is_procedural = true;
    cube_asset.generator = [](int) { return LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); };
    assets.push_back(cube_asset);

    ModelAsset sphere_asset;
    sphere_asset.name = "Sphere";
    sphere_asset.type = SPHERE;
    sphere_asset.is_procedural = true;
    sphere_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshSphere(1.0f, seg, seg)); };
    assets.push_back(sphere_asset);

    ModelAsset cone_asset;
    cone_asset.name = "Cone";
    cone_asset.type = CONE;
    cone_asset.is_procedural = true;
    cone_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, seg)); };
    assets.push_back(cone_asset);

    ModelAsset cylinder_asset;
    cylinder_asset.name = "Cylinder";
    cylinder_asset.type = CYLINDER;
    cylinder_asset.is_procedural = true;
    cylinder_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, seg)); };
    assets.push_back(cylinder_asset);

    ModelAsset hemisphere_asset;
    hemisphere_asset.name = "HemiSphere";
    hemisphere_asset.type = HEMISPHERE;
    hemisphere_asset.is_procedural = true;
    hemisphere_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshHemiSphere(1.0f, seg, seg)); };
    assets.push_back(hemisphere_asset);

    ModelAsset torus_asset;
    torus_asset.name = "Torus";
    torus_asset.type = TORUS;
    torus_asset.is_procedural = true;
    torus_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshTorus(1.0f, 1.0f, seg, seg)); };
    assets.push_back(torus_asset);
}

void update_model(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (!mesh || !mesh->asset || !mesh->asset->is_procedural || !mesh->asset->generator) return;
    if (mesh->model.meshCount > 0 && entity_owns_model(*e)) UnloadModel(mesh->model);

    int max_seg = 125;
    if (mesh->type == SPHERE || mesh->type == HEMISPHERE) max_seg = 100;

    if (mesh->segments < 3)        mesh->segments = 3;
    if (mesh->segments > max_seg)  mesh->segments = max_seg;

    mesh->model = mesh->asset->generator(mesh->segments);
    mesh->owns_model_instance = true;
}

void unload_models() {
    std::unordered_set<void*> released_meshes;

    for (auto& asset : assets) {
        if (!asset.is_procedural && asset.loaded_model.meshCount > 0 && asset.loaded_model.meshes) {
            void* mesh_ptr = static_cast<void*>(asset.loaded_model.meshes);
            if (released_meshes.insert(mesh_ptr).second) {
                UnloadModel(asset.loaded_model);
            }
        }
        asset.loaded_model = {0};
    }

    assets.clear();
}

void load_external_models(std::string project_path) {
    namespace fs = std::filesystem;

    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& path : collect_model_paths(resource_dir)) {
        const std::string asset_name = fs::relative(path, resource_dir).generic_string();
        bool already_loaded = false;
        for (const auto& existing : assets) {
            if (!existing.is_procedural && existing.name == asset_name) {
                already_loaded = true;
                break;
            }
        }
        if (already_loaded) continue;

        ModelAsset asset;
        asset.name = asset_name;
        asset.type = CUBE;
        asset.is_procedural = false;
        asset.filepath = path.string();

        bool loaded = false;
        asset.loaded_model = try_load_model(asset.filepath, loaded);
        if (!loaded) continue;

        assets.push_back(asset);
    }
}

void refresh_models(std::string project_path, Scene& scene) {
    namespace fs = std::filesystem;

    std::unordered_map<std::string, Model> old;

    for (auto& a : assets) {
        if (!a.is_procedural && a.loaded_model.meshCount > 0)
            old[a.name] = a.loaded_model;
    }

    std::vector<ModelAsset> next;

    for (auto& a : assets)
        if (a.is_procedural)
            next.push_back(a);

    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& path : collect_model_paths(resource_dir)) {
        std::string name = fs::relative(path, resource_dir).generic_string();

        ModelAsset a;
        a.name = name;
        a.type = CUBE;
        a.is_procedural = false;
        a.filepath = path.string();

        if (old.count(name)) {
            a.loaded_model = old[name];
            old.erase(name);
        } 
        
        else {
            bool loaded = false;
            a.loaded_model = try_load_model(a.filepath, loaded);
            if (!loaded) continue;
        }

        next.push_back(a);
    }

    for (auto& [_, m] : old) {
        UnloadModel(m);
    }

    assets = std::move(next);

    for (auto& entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || mesh->asset_name.empty()) continue;
        mesh->asset = nullptr;
        for (auto& a : assets) {
            if (a.name == mesh->asset_name) {
                mesh->asset = &a;
                break;
            }
        }
    }
}
