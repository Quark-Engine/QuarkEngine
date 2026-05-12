#pragma once

#include "QuarkCore/QuarkCore.hpp"
using namespace qc;
#include <vector>

struct EditableVertex {
    Vec3 position;
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