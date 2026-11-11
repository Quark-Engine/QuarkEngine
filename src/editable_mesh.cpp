#include "editable_mesh.h"

#include <algorithm>
#include <limits>
SEditableMeshBuildData BuildEditableMeshData(const CEditableMesh& editableMesh)
{
    SEditableMeshBuildData buildData;
    const size_t vertexCount = editableMesh.m_vVertices.size();
    const size_t triangleCount = editableMesh.m_vTriangles.size();
    if (vertexCount > static_cast<size_t>(std::numeric_limits<unsigned short>::max()) + 1)
    {
        buildData.m_IsValid = false;
        return buildData;
    }

    buildData.m_vVertices.resize(vertexCount * 3);
    buildData.m_vNormals.assign(vertexCount * 3, 0.0f);
    buildData.m_vTexcoords.resize(vertexCount * 2);
    buildData.m_vIndices.reserve(triangleCount * 3);

    for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const SEditableVertex& vertex = editableMesh.m_vVertices[vertexIndex];
        buildData.m_vVertices[vertexIndex * 3] = vertex.m_Position.x;
        buildData.m_vVertices[vertexIndex * 3 + 1] = vertex.m_Position.y;
        buildData.m_vVertices[vertexIndex * 3 + 2] = vertex.m_Position.z;
        buildData.m_vTexcoords[vertexIndex * 2] = vertex.m_U;
        buildData.m_vTexcoords[vertexIndex * 2 + 1] = vertex.m_V;
    }

    for (const SEditableTriangle& triangle : editableMesh.m_vTriangles)
    {
        if (triangle.m_A < 0 || triangle.m_B < 0 || triangle.m_C < 0 ||
            static_cast<size_t>(triangle.m_A) >= vertexCount ||
            static_cast<size_t>(triangle.m_B) >= vertexCount ||
            static_cast<size_t>(triangle.m_C) >= vertexCount)
        {
            buildData.m_IsValid = false;
            return buildData;
        }

        const unsigned short aIndices[] = {
            static_cast<unsigned short>(triangle.m_A),
            static_cast<unsigned short>(triangle.m_B),
            static_cast<unsigned short>(triangle.m_C)
        };
        buildData.m_vIndices.insert(buildData.m_vIndices.end(), std::begin(aIndices), std::end(aIndices));

        const Vec3 a = editableMesh.m_vVertices[triangle.m_A].m_Position;
        const Vec3 b = editableMesh.m_vVertices[triangle.m_B].m_Position;
        const Vec3 c = editableMesh.m_vVertices[triangle.m_C].m_Position;
        const Vec3 normal = (b - a).cross(c - a).normalized();
        for (int vertexIndex : { triangle.m_A, triangle.m_B, triangle.m_C })
        {
            buildData.m_vNormals[static_cast<size_t>(vertexIndex) * 3] += normal.x;
            buildData.m_vNormals[static_cast<size_t>(vertexIndex) * 3 + 1] += normal.y;
            buildData.m_vNormals[static_cast<size_t>(vertexIndex) * 3 + 2] += normal.z;
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        const size_t offset = vertexIndex * 3;
        const Vec3 normal = Vec3(
                                buildData.m_vNormals[offset],
                                buildData.m_vNormals[offset + 1],
                                buildData.m_vNormals[offset + 2])
                                .normalized();
        buildData.m_vNormals[offset] = normal.x;
        buildData.m_vNormals[offset + 1] = normal.y;
        buildData.m_vNormals[offset + 2] = normal.z;
    }

    return buildData;
}

void UploadEditableMeshData(Model& model, const SEditableMeshBuildData& buildData)
{
    if (!buildData.m_IsValid)
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

    mesh.vertexCount = static_cast<int>(buildData.m_vVertices.size() / 3);
    mesh.triangleCount = static_cast<int>(buildData.m_vIndices.size() / 3);
    if (mesh.vertexCount == 0 || mesh.triangleCount == 0)
    {
        return;
    }

    mesh.vertices = new float[buildData.m_vVertices.size()];
    mesh.normals = new float[buildData.m_vNormals.size()];
    mesh.texcoords = new float[buildData.m_vTexcoords.size()];
    mesh.indices = new unsigned short[buildData.m_vIndices.size()];
    std::copy(buildData.m_vVertices.begin(), buildData.m_vVertices.end(), mesh.vertices);
    std::copy(buildData.m_vNormals.begin(), buildData.m_vNormals.end(), mesh.normals);
    std::copy(buildData.m_vTexcoords.begin(), buildData.m_vTexcoords.end(), mesh.texcoords);
    std::copy(buildData.m_vIndices.begin(), buildData.m_vIndices.end(), mesh.indices);

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
