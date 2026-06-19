#pragma once

namespace mini
{

struct TransformComponent
{
    float x  = 0.0f, y  = 0.0f, z  = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
};

} // namespace mini