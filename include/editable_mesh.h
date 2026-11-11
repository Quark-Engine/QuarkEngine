#ifndef __EDITABLE_MESH_H__
#define __EDITABLE_MESH_H__

#include "QuarkCore/QuarkCore.hpp"
#include <vector>

struct SEditableVertex
{
    Vec3 m_Position;
    float m_U = 0.0f;
    float m_V = 0.0f;
};

struct SEditableTriangle
{
    int m_A;
    int m_B;
    int m_C;
};

class CEditableMesh
{
public:
    std::vector<SEditableVertex> m_vVertices;
    std::vector<SEditableTriangle> m_vTriangles;
};

struct SEditableMeshBuildData
{
    std::vector<float> m_vVertices;
    std::vector<float> m_vNormals;
    std::vector<float> m_vTexcoords;
    std::vector<unsigned short> m_vIndices;
    bool m_IsValid = true;
};

SEditableMeshBuildData BuildEditableMeshData(const CEditableMesh& editableMesh);
void UploadEditableMeshData(Model& model, const SEditableMeshBuildData& buildData);
void RebuildMeshFromEditable(Model& model, CEditableMesh& editableMesh);

#endif // __EDITABLE_MESH_H__
