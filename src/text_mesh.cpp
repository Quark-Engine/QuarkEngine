#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <numeric>
#include <filesystem>
#include <cmath>
#include <string>
#include <algorithm>
#include <cctype>
#include <memory>

#if _WIN32
#include <cstdlib>

#elif __APPLE__
#include <CoreText/CoreText.h>
#include <CoreFoundation/CoreFoundation.h>

#endif

#include "text_mesh.h"

namespace fs = std::filesystem;

struct SFTContour
{
    std::vector<Vector2> m_vPoints;
};

struct SFTOutlineCtx
{
    std::vector<SFTContour> m_vContours;
    Vector2 m_Current = { 0, 0 };
    float m_Scale = 1.0f;
};

static std::string LowercaseCopy(const std::string& str)
{
    std::string result = str;

    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
                   {
                       return std::tolower(c);
                   });

    return result;
}

CFreetypeTextMesh::~CFreetypeTextMesh()
{
    Unload();
}

void CFreetypeTextMesh::Init()
{
    if (m_pLibrary)
    {
        return;
    }

    FT_Library pLibrary = nullptr;
    if (FT_Init_FreeType(&pLibrary))
    {
        TraceLog(LogLevel::Error, "Freetype", "failed to init");
        return;
    }
    m_pLibrary = pLibrary;
}

void CFreetypeTextMesh::Unload()
{
    if (m_pLibrary)
    {
        FT_Done_FreeType(m_pLibrary);
        m_pLibrary = nullptr;
    }
}

bool CFreetypeTextMesh::IsReady() const
{
    return m_pLibrary != nullptr;
}

std::vector<std::pair<std::string, std::string>> CFreetypeTextMesh::GetSystemFonts()
{
    std::vector<std::pair<std::string, std::string>> vResult;

    auto scanDir = [&](const fs::path& dir)
    {
        if (!fs::exists(dir))
        {
            return;
        }
        std::error_code ec;

        for (auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec))
        {
            if (!entry.is_regular_file(ec))
            {
                continue;
            }

            auto ext = LowercaseCopy(entry.path().extension().string());
            if (ext != ".ttf" && ext != ".otf")
            {
                continue;
            }

            std::string name = entry.path().stem().string();
            vResult.push_back({ name, entry.path().string() });
        }
    };

#if _WIN32
    char aWindir[512] = {};
    size_t windirLength = 0;
    char* pWin = nullptr;
    if (_dupenv_s(&pWin, &windirLength, "WINDIR") == 0 && pWin)
    {
        snprintf(aWindir, sizeof(aWindir), "%s", pWin);
        free(pWin);
        pWin = aWindir;
    }

    if (pWin)
    {
        scanDir(std::string(pWin) + "\\Fonts");
    }

#elif __APPLE__
    scanDir("/System/Library/Fonts");
    scanDir("/Library/Fonts");

    const char* pHome = getenv("HOME");
    if (pHome)
    {
        scanDir(std::string(pHome) + "/Library/Fonts");
    }

#else
    scanDir("/usr/share/fonts");
    scanDir("/usr/local/share/fonts");

    const char* pHome = getenv("HOME");
    if (pHome)
    {
        scanDir(std::string(pHome) + "/.fonts");
    }
    if (pHome)
    {
        scanDir(std::string(pHome) + "/.local/share/fonts");
    }

#endif

    std::sort(vResult.begin(), vResult.end(), [](auto& a, auto& b)
              {
                  return a.first < b.first;
              });
    return vResult;
}

static float Cross2(Vector2 origin, Vector2 a, Vector2 b)
{
    return (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x);
}

static void PushQuadBezier(std::vector<Vector2>& vOut, Vector2 point0, Vector2 point1, Vector2 point2, int steps = 8)
{
    for (int i = 1; i <= steps; i++)
    {
        float t = (float)i / steps;
        float it = 1.f - t;

        vOut.push_back({ it * it * point0.x + 2 * it * t * point1.x + t * t * point2.x,
                         it * it * point0.y + 2 * it * t * point1.y + t * t * point2.y });
    }
}

static void PushCubicBezier(std::vector<Vector2>& vOut, Vector2 point0, Vector2 point1, Vector2 point2, Vector2 point3, int steps = 8)
{
    for (int i = 1; i <= steps; i++)
    {
        float t = (float)i / steps, it = 1.f - t;
        vOut.push_back({ it * it * it * point0.x + 3 * it * it * t * point1.x + 3 * it * t * t * point2.x + t * t * t * point3.x,
                         it * it * it * point0.y + 3 * it * it * t * point1.y + 3 * it * t * t * point2.y + t * t * t * point3.y });
    }
}

static float PolygonSignedArea(const std::vector<Vector2>& vPoints)
{
    float a = 0;
    int n = (int)vPoints.size();

    for (int i = 0; i < n; i++)
    {
        int j = (i + 1) % n;
        a += vPoints[i].x * vPoints[j].y - vPoints[j].x * vPoints[i].y;
    }

    return a * .5f;
}

static std::vector<int> EarClip(const std::vector<Vector2>& vPoints)
{
    std::vector<int> vResult;
    int n = (int)vPoints.size();
    if (n < 3)
    {
        return vResult;
    }

    std::vector<int> vIndices(n);
    std::iota(vIndices.begin(), vIndices.end(), 0);

    float a = 0;
    for (int i = 0; i < n; i++)
    {
        int j = (i + 1) % n;
        a += vPoints[i].x * vPoints[j].y - vPoints[j].x * vPoints[i].y;
    }

    if (a < 0)
    {
        std::reverse(vIndices.begin(), vIndices.end());
    }

    auto pointInTriangle = [&](Vector2 p, Vector2 a, Vector2 b, Vector2 c)
    {
        return Cross2(a, b, p) >= 0 && Cross2(b, c, p) >= 0 && Cross2(c, a, p) >= 0;
    };

    int safety = n * n + 10;
    int i = 0;

    while ((int)vIndices.size() > 3 && safety-- > 0)
    {
        int sz = (int)vIndices.size();
        int prev = (i - 1 + sz) % sz;
        int next = (i + 1) % sz;

        Vector2 a = vPoints[vIndices[prev]], b = vPoints[vIndices[i]], c = vPoints[vIndices[next]];
        bool ear = Cross2(a, b, c) > 0;
        if (ear)
        {
            for (int k = 0; k < sz && ear; k++)
            {
                if (k == prev || k == i || k == next)
                {
                    continue;
                }
                if (pointInTriangle(vPoints[vIndices[k]], a, b, c))
                {
                    ear = false;
                }
            }
        }

        if (ear)
        {
            vResult.push_back(vIndices[prev]);
            vResult.push_back(vIndices[i]);
            vResult.push_back(vIndices[next]);
            vIndices.erase(vIndices.begin() + i);
            sz--;
            if (i >= sz)
            {
                i = 0;
            }
        }
        else
        {
            i = (i + 1) % sz;
        }
    }

    if ((int)vIndices.size() == 3)
    {
        vResult.push_back(vIndices[0]);
        vResult.push_back(vIndices[1]);
        vResult.push_back(vIndices[2]);
    }

    return vResult;
}

static int FtMoveTo(const FT_Vector* pTo, void* pUser)
{
    SFTOutlineCtx* pCtx = (SFTOutlineCtx*)pUser;

    pCtx->m_vContours.push_back({});
    pCtx->m_Current = { (float)pTo->x * pCtx->m_Scale, (float)pTo->y * pCtx->m_Scale };
    pCtx->m_vContours.back().m_vPoints.push_back(pCtx->m_Current);

    return 0;
}

static int FtLineTo(const FT_Vector* pTo, void* pUser)
{
    SFTOutlineCtx* pCtx = (SFTOutlineCtx*)pUser;

    pCtx->m_Current = { (float)pTo->x * pCtx->m_Scale, (float)pTo->y * pCtx->m_Scale };

    if (!pCtx->m_vContours.empty())
    {
        pCtx->m_vContours.back().m_vPoints.push_back(pCtx->m_Current);
    }

    return 0;
}

static int FtConicTo(const FT_Vector* pCtrl, const FT_Vector* pTo, void* pUser)
{
    SFTOutlineCtx* pCtx = (SFTOutlineCtx*)pUser;
    if (pCtx->m_vContours.empty())
    {
        return 0;
    }

    Vector2 point1 = { (float)pCtrl->x * pCtx->m_Scale, (float)pCtrl->y * pCtx->m_Scale };
    Vector2 point2 = { (float)pTo->x * pCtx->m_Scale, (float)pTo->y * pCtx->m_Scale };

    PushQuadBezier(pCtx->m_vContours.back().m_vPoints, pCtx->m_Current, point1, point2);
    pCtx->m_Current = point2;

    return 0;
}

static int FtCubicTo(const FT_Vector* pC1, const FT_Vector* pC2, const FT_Vector* pTo, void* pUser)
{
    auto* pCtx = (SFTOutlineCtx*)pUser;
    if (pCtx->m_vContours.empty())
    {
        return 0;
    }

    Vector2 point1 = { (float)pC1->x * pCtx->m_Scale, (float)pC1->y * pCtx->m_Scale };
    Vector2 point2 = { (float)pC2->x * pCtx->m_Scale, (float)pC2->y * pCtx->m_Scale };
    Vector2 point3 = { (float)pTo->x * pCtx->m_Scale, (float)pTo->y * pCtx->m_Scale };

    PushCubicBezier(pCtx->m_vContours.back().m_vPoints, pCtx->m_Current, point1, point2, point3);
    pCtx->m_Current = point3;

    return 0;
}

static const FT_Outline_Funcs s_FtOutlineFuncs = {
    FtMoveTo, FtLineTo, FtConicTo, FtCubicTo, 0, 0
};

struct SMeshBuilder
{
    std::vector<float> m_vVerts;
    std::vector<float> m_vNorms;
    std::vector<float> m_vUvs;
    std::vector<unsigned short> m_vIndices;
    int m_Base = 0;

    void AddVertex(float x, float y, float z, float nx, float ny, float nz, float u, float v)
    {
        m_vVerts.insert(m_vVerts.end(), { x, y, z });
        m_vNorms.insert(m_vNorms.end(), { nx, ny, nz });
        m_vUvs.insert(m_vUvs.end(), { u, v });
    }

    void AddFace(const std::vector<Vector2>& vContour, float z, float normalZ, bool flipWinding)
    {
        auto vTris = EarClip(vContour);
        int n = (int)vContour.size();

        for (int i = 0; i < n; i++)
        {
            AddVertex(vContour[i].x, vContour[i].y, z, 0, 0, normalZ, vContour[i].x, vContour[i].y);
        }

        for (int k = 0; k + 2 < (int)vTris.size(); k += 3)
        {
            int a = m_Base + vTris[k];
            int b = m_Base + vTris[k + 1];
            int c = m_Base + vTris[k + 2];

            if (flipWinding)
            {
                std::swap(b, c);
            }

            m_vIndices.push_back((unsigned short)a);
            m_vIndices.push_back((unsigned short)b);
            m_vIndices.push_back((unsigned short)c);
        }

        m_Base += n;
    }

    void AddWall(const std::vector<Vector2>& vContour, float depth)
    {
        int n = (int)vContour.size();

        for (int i = 0; i < n; i++)
        {
            int j = (i + 1) % n;
            Vector2 a = vContour[i], b = vContour[j];

            float ex = b.y - a.y, ey = -(b.x - a.x);
            float len = sqrtf(ex * ex + ey * ey);

            if (len > 0.00001f)
            {
                ex /= len;
                ey /= len;
            }

            int v0 = m_Base;
            AddVertex(a.x, a.y, 0, ex, ey, 0, 0, 0);
            AddVertex(b.x, b.y, 0, ex, ey, 0, 1, 0);
            AddVertex(b.x, b.y, depth, ex, ey, 0, 1, 1);
            AddVertex(a.x, a.y, depth, ex, ey, 0, 0, 1);
            m_Base += 4;

            m_vIndices.push_back(v0);
            m_vIndices.push_back(v0 + 1);
            m_vIndices.push_back(v0 + 2);
            m_vIndices.push_back(v0);
            m_vIndices.push_back(v0 + 2);
            m_vIndices.push_back(v0 + 3);
        }
    }

    Mesh Build()
    {
        Mesh m = { 0 };
        if (m_vVerts.empty())
        {
            return m;
        }

        m.vertexCount = (int)(m_vVerts.size() / 3);
        m.triangleCount = (int)(m_vIndices.size() / 3);

        m.vertices = (float*)malloc((unsigned int)m_vVerts.size() * sizeof(float));
        m.normals = (float*)malloc((unsigned int)m_vNorms.size() * sizeof(float));
        m.texcoords = (float*)malloc((unsigned int)m_vUvs.size() * sizeof(float));
        m.indices = (unsigned short*)malloc((unsigned int)m_vIndices.size() * sizeof(unsigned short));

        memcpy(m.vertices, m_vVerts.data(), m_vVerts.size() * sizeof(float));
        memcpy(m.normals, m_vNorms.data(), m_vNorms.size() * sizeof(float));
        memcpy(m.texcoords, m_vUvs.data(), m_vUvs.size() * sizeof(float));
        memcpy(m.indices, m_vIndices.data(), m_vIndices.size() * sizeof(unsigned short));

        UploadMesh(&m, false);
        return m;
    }
};

Model CFreetypeTextMesh::Generate(const std::string& text, float size, float thickness, float letterSpacing, const std::string& fontPath) const
{
    auto makeFallback = []()
    {
        return LoadModelFromMesh(GenMeshCube(0.001f, 0.001f, 0.001f));
    };

    if (!m_pLibrary)
    {
        TraceLog(LogLevel::Warn, "Freetype", "not initialised");
        return makeFallback();
    }
    if (text.empty() || fontPath.empty())
    {
        return makeFallback();
    }

    FT_Face pFace;
    if (FT_New_Face(m_pLibrary, fontPath.c_str(), 0, &pFace))
    {
        TraceLog(LogLevel::Warn, "Freetype", TextFormat("cannot load font %s", fontPath.c_str()));
        return makeFallback();
    }

    const int resolution = 128;
    FT_Set_Pixel_Sizes(pFace, 0, resolution);
    float scale = size / (float)resolution;

    SMeshBuilder builder;
    float cursorX = 0.f;

    for (unsigned char ch : text)
    {
        if (FT_Load_Char(pFace, ch, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING))
        {
            continue;
        }

        FT_GlyphSlot pSlot = pFace->glyph;
        if (pSlot->format != FT_GLYPH_FORMAT_OUTLINE)
        {
            cursorX += (pSlot->advance.x >> 6) * scale + letterSpacing;
            continue;
        }

        SFTOutlineCtx outlineCtx;
        outlineCtx.m_Scale = scale;
        FT_Outline_Decompose(&pSlot->outline, &s_FtOutlineFuncs, &outlineCtx);

        for (auto& contour : outlineCtx.m_vContours)
        {
            for (auto& point : contour.m_vPoints)
            {
                point.x += cursorX;
            }
        }

        for (auto& contour : outlineCtx.m_vContours)
        {
            if (contour.m_vPoints.size() < 3)
            {
                continue;
            }
            float area = PolygonSignedArea(contour.m_vPoints);
            bool isHole = area < 0;

            builder.AddFace(contour.m_vPoints, thickness, 1.f, isHole);
            builder.AddFace(contour.m_vPoints, 0.f, -1.f, !isHole);
            builder.AddWall(contour.m_vPoints, thickness);
        }

        cursorX += (pSlot->advance.x >> 6) * scale + letterSpacing;
    }

    FT_Done_Face(pFace);

    Mesh mesh = builder.Build();
    if (mesh.vertexCount == 0)
    {
        return makeFallback();
    }

    float halfWidth = cursorX * 0.5f;
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[i * 3] -= halfWidth;
    }

    UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);

    Model model = LoadModelFromMesh(mesh);

    if (model.materialCount == 0)
    {
        model.materials = (Material*)malloc(sizeof(Material));
        model.materials[0] = LoadMaterialDefault();
        model.materialCount = 1;
    }
    return model;
}

std::string CFreetypeTextMesh::GetDefaultFontPath()
{
    auto vFonts = GetSystemFonts();
    if (!vFonts.empty())
    {
        return vFonts[0].second;
    }
    return "";
}
