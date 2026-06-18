#pragma once
 
namespace mini
{
 
struct HealthComponent
{
    float current = 100.0f;
    float maximum = 100.0f;
 
    [[nodiscard]] bool isDead()      const { return current <= 0.0f; }
    [[nodiscard]] float getNormalized() const { return maximum > 0.0f ? current / maximum : 0.0f; }
};
 
} // namespace mini