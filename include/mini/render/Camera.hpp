#pragma once

#include <glm/glm.hpp>

namespace mini
{

// Camera con proiezione prospettica.
// Produce le matrici View e Projection necessarie per trasformare
// le posizioni 3D del world space in coordinate clip dello schermo.
class Camera
{
public:
    Camera(float fovDeg, float aspect, float nearPlane, float farPlane);

    // Posizione e orientamento
    void setPosition(const glm::vec3& pos);
    void lookAt(const glm::vec3& target, const glm::vec3& up = {0.0f, 1.0f, 0.0f});
    void setAspect(float aspect);

    // Matrici
    [[nodiscard]] glm::mat4 getView()           const;
    [[nodiscard]] glm::mat4 getProjection()     const;
    [[nodiscard]] glm::mat4 getViewProjection() const;

    [[nodiscard]] const glm::vec3& getPosition() const;
    [[nodiscard]] float            getFov()       const;

private:
    glm::vec3 m_position = {0.0f, 0.0f,  3.0f};
    glm::vec3 m_target   = {0.0f, 0.0f,  0.0f};
    glm::vec3 m_up       = {0.0f, 1.0f,  0.0f};

    float m_fov;
    float m_aspect;
    float m_near;
    float m_far;
};

} // namespace mini