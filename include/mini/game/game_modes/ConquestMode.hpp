#pragma once

namespace mini
{

class World;
class Mesh;
class Texture;

class ConquestMode
{
public:
    // mesh e texture: non-owning, posseduti da Application
    void start(World& world, Mesh* mesh, Texture* texture);
    void update(World& world, float deltaTime);
};

} // namespace mini