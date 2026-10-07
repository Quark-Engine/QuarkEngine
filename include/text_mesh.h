#ifndef __TEXT_MESH_H__
#define __TEXT_MESH_H__

#include "QuarkCore/QuarkCore.hpp"

#include <string>
#include <utility>
#include <vector>

struct FT_LibraryRec_;

class CFreetypeTextMesh
{
public:
    CFreetypeTextMesh() = default;
    ~CFreetypeTextMesh();

    CFreetypeTextMesh(const CFreetypeTextMesh&) = delete;
    CFreetypeTextMesh& operator=(const CFreetypeTextMesh&) = delete;

    void Init();

    void Unload();

    bool IsReady() const;

    Model Generate(const std::string& text, float size, float thickness,
                       float letterSpacing, const std::string& fontPath) const;

    static std::vector<std::pair<std::string, std::string>> GetSystemFonts();

    static std::string GetDefaultFontPath();

private:
    FT_LibraryRec_* m_pLibrary = nullptr;
};

#endif // __TEXT_MESH_H__
