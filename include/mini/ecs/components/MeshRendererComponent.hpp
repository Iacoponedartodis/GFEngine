#pragma once

namespace mini
{
class Mesh;
class Texture;

struct MeshRendererComponent
{
    Mesh*    mesh    = nullptr;
    Texture* texture = nullptr;
    float    r       = 1.0f;
    float    g       = 1.0f;
    float    b       = 1.0f;
    bool     visible = true;
};

} // namespace mini