#pragma once
#include "mini/ecs/ISystem.hpp"

namespace mini
{

// AiSystem: ogni entita' con AiComponent + TeamComponent spara
// verso le entita' del team avversario entro aggroRange.
// I proiettili AI volano a velocita' piu' bassa di quelli del giocatore.
class AiSystem : public ISystem
{
public:
    void update(World& world, float dt) override;

    static constexpr float k_bulletSpeed = 8.0f;
    static constexpr float k_bulletDmg   = 20.0f;
    static constexpr float k_bulletLife  = 5.0f;
};

} // namespace mini