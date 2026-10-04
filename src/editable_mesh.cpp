#include "editable_mesh.h"

#include <algorithm>
#include <limits>

using namespace qc;

SEditableMeshBuildData BuildEditableMeshData(const CEditableMesh& editableMesh)
{
    SEditableMeshBuildData buildData;
    const size_t vertexCount = editableMesh.m_vVertices.size();
    const size_t triangleCount = editableMesh.m_vTriangles.size();
    if (vertexCount > static_cast<size_t>(std::numeric_limits<unsigned short>::max()) + 1)
    {
        buildData.IsValid = false;
        return buildData;
    }

    buildData.vVertices.resize(vertexCount * 3);
    buildData.vNormals.assign(vertexCount * 3, 0.0f);
    buildData.vTexcoords.resize(vertexCount * 2);
    buildData.vIndices.reserve(triangleCount * 3);

    for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const SEditableVertex& vertex = editableMesh.m_vVertices[vertexIndex];
        buildData.vVertices[vertexIndex * 3] = vertex.Position.x;
        buildData.vVertices[vertexIndex * 3 + 1] = vertex.Position.y;
        buildData.vVertices[vertexIndex * 3 + 2] = vertex.Position.z;
        buildData.vTexcoords[vertexIndex * 2] = vertex.U;
        buildData.vTexcoords[vertexIndex * 2 + 1] = vertex.V;
    }

    for (const SEditableTriangle& triangle : editableMesh.m_vTriangles)
    {
        if (triangle.A < 0 || triangle.B < 0 || triangle.C < 0 ||
            static_cast<size_t>(triangle.A) >= vertexCount ||
            static_cast<size_t>(triangle.B) >= vertexCount ||
            static_cast<size_t>(triangle.C) >= vertexCount)
        {
            buildData.IsValid = false;
            return buildData;
        }

        const unsigned short indices[] = {
            static_cast<unsigned short>(triangle.A),
            static_cast<unsigned short>(triangle.B),
            static_cast<unsigned short>(triangle.C)
        };
        buildData.vIndices.insert(buildData.vIndices.end(), std::begin(indices), std::end(indices));

        const Vec3 a = editableMesh.m_vVertices[triangle.A].Position;
        const Vec3 b = editableMesh.m_vVertices[triangle.B].Position;
        const Vec3 c = editableMesh.m_vVertices[triangle.C].Position;
        const Vec3 normal = (b - a).cross(c - a).normalized();
        for (int vertexIndex : { triangle.A, triangle.B, triangle.C })
        {
            buildData.vNormals[static_cast<size_t>(vertexIndex) * 3] += normal.x;
            buildData.vNormals[static_cast<size_t>(vertexIndex) * 3 + 1] += normal.y;
            buildData.vNormals[static_cast<size_t>(vertexIndex) * 3 + 2] += normal.z;
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const size_t offset = vertexIndex * 3;
        const Vec3 normal = Vec3(
            buildData.vNormals[offset],
            buildData.vNormals[offset + 1],
            buildData.vNormals[offset + 2]
        ).normalized();
        buildData.vNormals[offset] = normal.x;
        buildData.vNormals[offset + 1] = normal.y;
        buildData.vNormals[offset + 2] = normal.z;
    }

    return buildData;
}

void UploadEditableMeshData(Model& model, const SEditableMeshBuildData& buildData)
{
    if (!buildData.IsValid)
    {
        return;
    }

    if (model.meshCount <= 0)
    {
        model.meshCount = 1;
        model.meshes = new Mesh[1]{};
    }

    Mesh& mesh = model.meshes[0];
    if (mesh.vaoId > 0 || mesh.vertices || mesh.normals || mesh.texcoords || mesh.indices)
    {
        UnloadMesh(mesh);
        mesh = {};
    }

    mesh.vertexCount = static_cast<int>(buildData.vVertices.size() / 3);
    mesh.triangleCount = static_cast<int>(buildData.vIndices.size() / 3);
    if (mesh.vertexCount == 0 || mesh.triangleCount == 0)
    {
        return;
    }

    mesh.vertices = new float[buildData.vVertices.size()];
    mesh.normals = new float[buildData.vNormals.size()];
    mesh.texcoords = new float[buildData.vTexcoords.size()];
    mesh.indices = new unsigned short[buildData.vIndices.size()];
    std::copy(buildData.vVertices.begin(), buildData.vVertices.end(), mesh.vertices);
    std::copy(buildData.vNormals.begin(), buildData.vNormals.end(), mesh.normals);
    std::copy(buildData.vTexcoords.begin(), buildData.vTexcoords.end(), mesh.texcoords);
    std::copy(buildData.vIndices.begin(), buildData.vIndices.end(), mesh.indices);

    UploadMesh(&mesh, true);

    if (model.materialCount <= 0)
    {
        model.materialCount = 1;
        model.materials = new Material[1];
        model.materials[0] = LoadMaterialDefault();
    }

    if (!model.meshMaterial)
    {
        model.meshMaterial = new int[1];
    }
    model.meshMaterial[0] = 0;
}

void RebuildMeshFromEditable(Model& model, CEditableMesh& editableMesh)
{
    UploadEditableMeshData(model, BuildEditableMeshData(editableMesh));
}
