#pragma once

namespace mini
{
    struct TransformComponent
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct TeamComponent
    {
        int teamId = 0;
    };

    struct VelocityComponent
    {
        float vx = 0.0f;
        float vy = 0.0f;
    };
}