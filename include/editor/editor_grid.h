#ifndef __EDITOR_GRID_H__
#define __EDITOR_GRID_H__

#include "QuarkCore/QuarkCore.hpp"

class CInfiniteGrid
{
public:
    CInfiniteGrid() = delete;

    static void Draw(const qc::Camera3D& camera, int viewportWidth, int viewportHeight,
        float spacing, qc::Color color, float maxExtent = 1000.0f);
};

#endif // __EDITOR_GRID_H__