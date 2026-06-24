#pragma once

#include <glm/glm.hpp>

namespace mini
{

// Camera FPS con yaw/pitch.
class Camera
{
public:
    Camera(float fovDeg, float aspect, float nearPlane, float farPlane);

    void setPosition(const glm::vec3& pos);
    void lookAt(const glm::vec3& target, const glm::vec3& up = {0.0f, 1.0f, 0.0f});
    void setAspect(float aspect);
    void setSpeed(float speed);
    void setFov(float fovDeg);

    void processKeyboard(bool fwd, bool bwd, bool lft, bool rgt,
                         bool moveUp, bool moveDown, float dt);
    void processMouse(float dx, float dy, float sensitivity = 0.1f);

    [[nodiscard]] glm::mat4 getView()           const;
    [[nodiscard]] glm::mat4 getProjection()     const;
    [[nodiscard]] glm::mat4 getViewProjection() const;

    [[nodiscard]] const glm::vec3& getPosition() const;
    [[nodiscard]] glm::vec3        getForward()  const;
    [[nodiscard]] float            getYaw()      const { return m_yaw; }
    [[nodiscard]] float            getPitch()    const { return m_pitch; }
    [[nodiscard]] float            getFov()      const;

private:
    glm::vec3 m_position = {0.0f, 0.0f,  3.0f};
    glm::vec3 m_front    = {0.0f, 0.0f, -1.0f};
    glm::vec3 m_up       = {0.0f, 1.0f,  0.0f};

    float m_yaw   = -90.0f;
    float m_pitch =   0.0f;
    float m_speed =   5.0f;

    float m_fov, m_aspect, m_near, m_far;

    void updateFront();
};

} // namespace mini