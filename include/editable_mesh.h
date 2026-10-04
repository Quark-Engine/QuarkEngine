#ifndef __EDITABLE_MESH_H__
#define __EDITABLE_MESH_H__

#include "QuarkCore/QuarkCore.hpp"
#include <vector>

struct SEditableVertex
{
    qc::Vec3 Position;
    float U = 0.0f;
    float V = 0.0f;
};

struct SEditableTriangle
{
    int A;
    int B;
    int C;
};

class CEditableMesh
{
public:
    std::vector<SEditableVertex> m_vVertices;
    std::vector<SEditableTriangle> m_vTriangles;
};

struct SEditableMeshBuildData
{
    std::vector<float> vVertices;
    std::vector<float> vNormals;
    std::vector<float> vTexcoords;
    std::vector<unsigned short> vIndices;
    bool IsValid = true;
};

SEditableMeshBuildData BuildEditableMeshData(const CEditableMesh& editableMesh);
void UploadEditableMeshData(qc::Model& model, const SEditableMeshBuildData& buildData);
void RebuildMeshFromEditable(qc::Model& model, CEditableMesh& editableMesh);

#endif // __EDITABLE_MESH_H__
