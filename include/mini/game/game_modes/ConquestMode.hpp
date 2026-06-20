#pragma once
#include "mini/ecs/Entity.hpp"

namespace mini { class World; class Mesh; class Texture; }

namespace mini
{

class ConquestMode
{
public:
    void start(World& world, Mesh* mesh, Texture* texture);
    void update(World& world, float deltaTime);

    // Restituisce l'EntityId del player per sincronizzarlo con la camera
    [[nodiscard]] EntityId getPlayerEntity() const { return m_playerEntity; }

private:
    EntityId m_playerEntity = 0;
};

} // namespace mini