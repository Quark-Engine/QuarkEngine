#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <numeric>
#include <filesystem>
#include <cmath>
#include <string>
#include <algorithm>
#include <cctype>

#if _WIN32
    #include <cstdlib>

#elif __APPLE__
    #include <CoreText/CoreText.h>
    #include <CoreFoundation/CoreFoundation.h>

#endif

#include "headers/text_mesh.h"


namespace fs = std::filesystem;

FT_Library g_ft = nullptr;

struct FTContour { std::vector<Vector2> pts; };
struct FTOutlineCtx {
    std::vector<FTContour> contours;
    Vector2 current = {0, 0};
    float scale = 1.0f;
};

static std::string lowercase_copy(const std::string& str) {
    std::string result = str;

    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    return result;
}

void init_freetype() {
    if (FT_Init_FreeType(&g_ft)) {
        TraceLog(LOG_ERROR, "FreeType: failed to init");
    }
}

void shutdown_freetype() {
    if (g_ft) {
        FT_Done_FreeType(g_ft);
        g_ft = nullptr;
    }
}

std::vector<std::pair<std::string, std::string>> get_system_fonts() {
    std::vector<std::pair<std::string, std::string>> result;

    auto scan_dir = [&](const fs::path& dir) {
        if (!fs::exists(dir)) return;
        std::error_code ec;

        for (auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            
            auto ext = lowercase_copy(entry.path().extension().string());
            if (ext != ".ttf" && ext != ".otf") continue;

            std::string name = entry.path().stem().string();
            result.push_back({name, entry.path().string()});
        }
    };

#if _WIN32
    const char* win = getenv("WINDIR");

    if (win) {
        scan_dir(std::string(win) + "\\Fonts");
    }

#elif __APPLE__
    scan_dir("/System/Library/Fonts");
    scan_dir("/Library/Fonts");

    const char* home = getenv("HOME");
    if (home) scan_dir(std::string(home) + "/Library/Fonts");

#else
    scan_dir("/usr/share/fonts");
    scan_dir("/usr/local/share/fonts");
    
    const char* home = getenv("HOME");
    if (home) scan_dir(std::string(home) + "/.fonts");
    if (home) scan_dir(std::string(home) + "/.local/share/fonts");

    #endif

    std::sort(result.begin(), result.end(), [](auto& a, auto& b) { return a.first < b.first; });
    return result;
}

static float cross2(Vector2 o, Vector2 a, Vector2 b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

static void push_quad_bezier(std::vector<Vector2>& out, Vector2 p0, Vector2 p1, Vector2 p2, int steps = 8) {
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        float it = 1.f - t;

       out.push_back({ 
            it*it*p0.x + 2*it*t*p1.x + t*t*p2.x,
            it*it*p0.y + 2*it*t*p1.y + t*t*p2.y
        });
    }
}

static void push_cubic_bezier(std::vector<Vector2>& out, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, int steps = 8) {
    for (int i = 1; i <= steps; i++) {
        float t = (float)i/steps, it = 1.f-t;
        out.push_back({
            it*it*it*p0.x + 3*it*it*t*p1.x + 3*it*t*t*p2.x + t*t*t*p3.x,
            it*it*it*p0.y + 3*it*it*t*p1.y + 3*it*t*t*p2.y + t*t*t*p3.y 
        });
    }
}

static float polygon_signed_area(const std::vector<Vector2>& p) {
    float a = 0;
    int n = (int)p.size();

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += p[i].x * p[j].y - p[j].x * p[i].y;
    }

    return a * .5f;
}

static std::vector<int> ear_clip(const std::vector<Vector2>& pts) {
    std::vector<int> result;
    int n = (int)pts.size();
    if (n < 3) return result;

    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);

    float a = 0;
    for (int i = 0; i < n; i++) {
        int j = (i+1)%n;
        a += pts[i].x*pts[j].y - pts[j].x*pts[i].y;
    }
    
    if (a < 0) std::reverse(idx.begin(), idx.end());

    auto point_in_tri = [&](Vector2 p, Vector2 a, Vector2 b, Vector2 c) {
        return cross2(a,b,p) >= 0 && cross2(b,c,p) >= 0 && cross2(c,a,p) >= 0;
    };

    int safety = n * n + 10;
    int i = 0;

    while ((int)idx.size() > 3 && safety-- > 0) {
        int sz   = (int)idx.size();
        int prev = (i - 1 + sz) % sz;
        int next = (i + 1) % sz;

        Vector2 a = pts[idx[prev]], b = pts[idx[i]], c = pts[idx[next]];
        bool ear = cross2(a, b, c) > 0;
        if (ear) {
            for (int k = 0; k < sz && ear; k++) {
                if (k == prev || k == i || k == next) continue;
                if (point_in_tri(pts[idx[k]], a, b, c)) ear = false;
            }
        }

        if (ear) {
            result.push_back(idx[prev]);
            result.push_back(idx[i]);
            result.push_back(idx[next]);
            idx.erase(idx.begin() + i);
            sz--;
            if (i >= sz) i = 0;
        } 
        
        else {
            i = (i + 1) % sz;
        }
    }

    if ((int)idx.size() == 3) {
        result.push_back(idx[0]);
        result.push_back(idx[1]);
        result.push_back(idx[2]);
    }

    return result;
}

static int ft_move_to(const FT_Vector* to, void* user) {
    FTOutlineCtx* ctx = (FTOutlineCtx*)user;

    ctx->contours.push_back({});
    ctx->current = { (float)to->x * ctx->scale, (float)to->y * ctx->scale };
    ctx->contours.back().pts.push_back(ctx->current);

    return 0;
}

static int ft_line_to(const FT_Vector* to, void* user) {
    FTOutlineCtx* ctx = (FTOutlineCtx*)user;

    ctx->current = { (float)to->x * ctx->scale, (float)to->y * ctx->scale };
    
    if (!ctx->contours.empty()) {
        ctx->contours.back().pts.push_back(ctx->current);
    }

    return 0;
}

static int ft_conic_to(const FT_Vector* ctrl, const FT_Vector* to, void* user) {
    FTOutlineCtx* ctx = (FTOutlineCtx*)user;
    if (ctx->contours.empty()) return 0;

    Vector2 p1 = { (float)ctrl->x * ctx->scale, (float)ctrl->y * ctx->scale };
    Vector2 p2 = { (float)to->x * ctx->scale, (float)to->y * ctx->scale };

    push_quad_bezier(ctx->contours.back().pts, ctx->current, p1, p2);
    ctx->current = p2;

    return 0;
}

static int ft_cubic_to(const FT_Vector* c1, const FT_Vector* c2, const FT_Vector* to, void* user) {
    auto* ctx = (FTOutlineCtx*)user;
    if (ctx->contours.empty()) return 0;

    Vector2 p1 = { (float)c1->x * ctx->scale, (float)c1->y * ctx->scale };
    Vector2 p2 = { (float)c2->x * ctx->scale, (float)c2->y * ctx->scale };
    Vector2 p3 = { (float)to->x * ctx->scale, (float)to->y * ctx->scale };
    
    push_cubic_bezier(ctx->contours.back().pts, ctx->current, p1, p2, p3);
    ctx->current = p3;

    return 0;
}

static const FT_Outline_Funcs g_ft_outline_funcs = {
    ft_move_to, ft_line_to, ft_conic_to, ft_cubic_to, 0, 0
};

struct MeshBuilder {
    std::vector<float>          verts;
    std::vector<float>          norms;
    std::vector<float>          uvs;
    std::vector<unsigned short> indices;
    int base = 0;

    void add_vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) {
        verts.insert(verts.end(), { x, y, z });
        norms.insert(norms.end(), { nx, ny, nz });
        uvs.insert(uvs.end(), { u, v });
    }

    void add_face(const std::vector<Vector2>& contour, float z, float normal_z, bool flip_winding) {
        auto tris = ear_clip(contour);
        int n = (int)contour.size();

        for (int i = 0; i < n; i++) {
            add_vertex(contour[i].x, contour[i].y, z, 0, 0, normal_z, contour[i].x, contour[i].y);
        }

        for (int k = 0; k + 2 < (int)tris.size(); k += 3) {
            int a = base + tris[k];
            int b = base + tris[k+1];
            int c = base + tris[k+2];

            if (flip_winding) std::swap(b, c);

            indices.push_back((unsigned short)a);
            indices.push_back((unsigned short)b);
            indices.push_back((unsigned short)c);
        }

        base += n;
    }

    void add_wall(const std::vector<Vector2>& contour, float depth) {
        int n = (int)contour.size();

        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            Vector2 a = contour[i], b = contour[j];

            float ex = b.y - a.y, ey = -(b.x - a.x);
            float len = sqrtf(ex*ex + ey*ey);

            if (len > 0.00001f) { ex /= len; ey /= len; }

            int v0 = base;
            add_vertex(a.x, a.y, 0,     ex, ey, 0, 0, 0);
            add_vertex(b.x, b.y, 0,     ex, ey, 0, 1, 0);
            add_vertex(b.x, b.y, depth, ex, ey, 0, 1, 1);
            add_vertex(a.x, a.y, depth, ex, ey, 0, 0, 1);
            base += 4;

            indices.push_back(v0);     indices.push_back(v0+1);
            indices.push_back(v0+2);   indices.push_back(v0);
            indices.push_back(v0+2);   indices.push_back(v0+3);
        }
    }

    Mesh build() {
        Mesh m = {0};
        if (verts.empty()) return m;

        m.vertexCount   = (int)(verts.size() / 3);
        m.triangleCount = (int)(indices.size() / 3);

        m.vertices  = (float*)MemAlloc((unsigned int)verts.size()   * sizeof(float));
        m.normals   = (float*)MemAlloc((unsigned int)norms.size()   * sizeof(float));
        m.texcoords = (float*)MemAlloc((unsigned int)uvs.size()     * sizeof(float));
        m.indices   = (unsigned short*)MemAlloc((unsigned int)indices.size() * sizeof(unsigned short));

        memcpy(m.vertices,  verts.data(),   verts.size()   * sizeof(float));
        memcpy(m.normals,   norms.data(),   norms.size()   * sizeof(float));
        memcpy(m.texcoords, uvs.data(),     uvs.size()     * sizeof(float));
        memcpy(m.indices,   indices.data(), indices.size() * sizeof(unsigned short));

        UploadMesh(&m, false);
        return m;
    }
};

Model generate_text_mesh(const std::string& text, float size, float thickness, float letter_spacing, const std::string& font_path)
{
    auto make_fallback = []() {
        return LoadModelFromMesh(GenMeshCube(0.001f, 0.001f, 0.001f));
    };

    if (!g_ft) { TraceLog(LOG_WARNING, "FreeType not initialised"); return make_fallback(); }
    if (text.empty() || font_path.empty()) return make_fallback();

    FT_Face face;
    if (FT_New_Face(g_ft, font_path.c_str(), 0, &face)) {
        TraceLog(LOG_WARNING, "FreeType: cannot load font %s", font_path.c_str());
        return make_fallback();
    }

    const int FT_RES = 128;
    FT_Set_Pixel_Sizes(face, 0, FT_RES);
    float scale = size / (float)FT_RES;

    MeshBuilder builder;
    float cursor_x = 0.f;

    for (unsigned char ch : text) {
        if (FT_Load_Char(face, ch, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) continue;

        FT_GlyphSlot slot = face->glyph;
        if (slot->format != FT_GLYPH_FORMAT_OUTLINE) {
            cursor_x += (slot->advance.x >> 6) * scale + letter_spacing;
            continue;
        }

        FTOutlineCtx ctx;
        ctx.scale = scale;
        FT_Outline_Decompose(&slot->outline, &g_ft_outline_funcs, &ctx);

        for (auto& c : ctx.contours) {
            for (auto& p : c.pts)
                p.x += cursor_x;
        }

        for (auto& c : ctx.contours) {
            if (c.pts.size() < 3) continue;
            float area = polygon_signed_area(c.pts);
            bool is_hole = area < 0;

            builder.add_face(c.pts, thickness, 1.f, is_hole);
            builder.add_face(c.pts, 0.f,       -1.f, !is_hole);
            builder.add_wall(c.pts, thickness);
        }

        cursor_x += (slot->advance.x >> 6) * scale + letter_spacing;
    }

    FT_Done_Face(face);

    Mesh mesh = builder.build();
    if (mesh.vertexCount == 0) return make_fallback();

    float half_w = cursor_x * 0.5f;
    for (int i = 0; i < mesh.vertexCount; i++)
        mesh.vertices[i * 3] -= half_w;

    UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);

    Model model = LoadModelFromMesh(mesh);

    if (model.materialCount == 0) {
        model.materials  = (Material*)MemAlloc(sizeof(Material));
        model.materials[0] = LoadMaterialDefault();
        model.materialCount = 1;
    }
    return model;
}

std::string get_default_font_path() {
    auto fonts = get_system_fonts();
    if (!fonts.empty()) return fonts[0].second;
    return "";
}