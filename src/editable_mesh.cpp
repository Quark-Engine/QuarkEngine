#include "editable_mesh.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <config.h>

void rebuild_mesh_from_editable(Model& model, EditableMesh& editable) {
    if (model.meshCount <= 0) {
        model.meshCount = 1;
        model.meshes = (Mesh*)MemAlloc(sizeof(Mesh));
        model.meshes[0] = {};
    }

    Mesh& mesh = model.meshes[0];

    if (mesh.vaoId > 0) {
        rlUnloadVertexArray(mesh.vaoId);
        if (mesh.vboId) {
            for (int i = 0; i < MAX_MESH_VERTEX_BUFFERS; i++) {
                if (mesh.vboId[i] > 0)
                    rlUnloadVertexBuffer(mesh.vboId[i]);
            }
        }

        MemFree(mesh.vertices);
        MemFree(mesh.normals);
        MemFree(mesh.texcoords);
        MemFree(mesh.indices);
        mesh = {};
    }

    mesh.vertexCount   = (int)editable.vertices.size();
    mesh.triangleCount = (int)editable.triangles.size();

    if (mesh.vertexCount == 0 || mesh.triangleCount == 0)
        return;

    mesh.vertices  = (float*)MemAlloc(sizeof(float) * mesh.vertexCount * 3);
    mesh.normals   = (float*)MemAlloc(sizeof(float) * mesh.vertexCount * 3);
    mesh.texcoords = (float*)MemAlloc(sizeof(float) * mesh.vertexCount * 2);
    mesh.indices   = (unsigned short*)MemAlloc(sizeof(unsigned short) * mesh.triangleCount * 3);

    for (int i = 0; i < mesh.vertexCount; i++) {
        Vector3 p = editable.vertices[i].position;
        mesh.vertices[i * 3 + 0] = p.x;
        mesh.vertices[i * 3 + 1] = p.y;
        mesh.vertices[i * 3 + 2] = p.z;

        mesh.normals[i * 3 + 0] = 0;
        mesh.normals[i * 3 + 1] = 1;
        mesh.normals[i * 3 + 2] = 0;

        mesh.texcoords[i * 2 + 0] = p.x;
        mesh.texcoords[i * 2 + 1] = p.z;
    }

    for (int i = 0; i < mesh.triangleCount; i++) {
        EditableTriangle& tri = editable.triangles[i];
        mesh.indices[i * 3 + 0] = (unsigned short)tri.a;
        mesh.indices[i * 3 + 1] = (unsigned short)tri.b;
        mesh.indices[i * 3 + 2] = (unsigned short)tri.c;
    }

    for (int i = 0; i < mesh.vertexCount * 3; i++)
        mesh.normals[i] = 0.0f;

    for (int i = 0; i < mesh.triangleCount; i++) {
        int ia = mesh.indices[i * 3 + 0];
        int ib = mesh.indices[i * 3 + 1];
        int ic = mesh.indices[i * 3 + 2];

        Vector3 a = { mesh.vertices[ia*3], mesh.vertices[ia*3+1], mesh.vertices[ia*3+2] };
        Vector3 b = { mesh.vertices[ib*3], mesh.vertices[ib*3+1], mesh.vertices[ib*3+2] };
        Vector3 c = { mesh.vertices[ic*3], mesh.vertices[ic*3+1], mesh.vertices[ic*3+2] };

        Vector3 n = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(b, a),
            Vector3Subtract(c, a)
        ));

        for (int v : {ia, ib, ic}) {
            mesh.normals[v*3+0] += n.x;
            mesh.normals[v*3+1] += n.y;
            mesh.normals[v*3+2] += n.z;
        }
    }

    for (int i = 0; i < mesh.vertexCount; i++) {
        Vector3 n = Vector3Normalize({
            mesh.normals[i*3+0],
            mesh.normals[i*3+1],
            mesh.normals[i*3+2]
        });

        mesh.normals[i*3+0] = n.x;
        mesh.normals[i*3+1] = n.y;
        mesh.normals[i*3+2] = n.z;
    }

    UploadMesh(&mesh, true);

    if (model.materialCount <= 0) {
        model.materialCount = 1;
        model.materials = (Material*)MemAlloc(sizeof(Material));
        model.materials[0] = LoadMaterialDefault();
    }

    if (!model.meshMaterial)
        model.meshMaterial = (int*)MemAlloc(sizeof(int));

    model.meshMaterial[0] = 0;
}