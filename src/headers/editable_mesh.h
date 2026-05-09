#pragma once

#include "raylib.h"
#include <vector>

struct EditableVertex {
    Vector3 position;
};

struct EditableTriangle {
    int a;
    int b;
    int c;
};

struct EditableMesh {
    std::vector<EditableVertex> vertices;
    std::vector<EditableTriangle> triangles;
};

void rebuild_mesh_from_editable(Model& model, EditableMesh& editable);