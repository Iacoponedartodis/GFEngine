#pragma once

#include <glm/glm.hpp>

namespace mini
{

// Camera FPS con yaw/pitch.
// Supporta sia lookAt (per setup iniziale) che movimento libero
// via processKeyboard e processMouse (per il gameplay).
class Camera
{
public:
    Camera(float fovDeg, float aspect, float nearPlane, float farPlane);

    // Setup
    void setPosition(const glm::vec3& pos);
    void lookAt(const glm::vec3& target, const glm::vec3& up = {0.0f, 1.0f, 0.0f});
    void setAspect(float aspect);
    void setSpeed(float speed);

    // Input FPS — da chiamare ogni frame
    void processKeyboard(bool fwd, bool bwd, bool lft, bool rgt,
                         bool moveUp, bool moveDown, float dt);
    void processMouse(float dx, float dy, float sensitivity = 0.1f);

    // Matrici per il renderer
    [[nodiscard]] glm::mat4 getView()           const;
    [[nodiscard]] glm::mat4 getProjection()     const;
    [[nodiscard]] glm::mat4 getViewProjection() const;

    [[nodiscard]] const glm::vec3& getPosition() const;
    [[nodiscard]] float            getFov()       const;

private:
    glm::vec3 m_position = {0.0f, 0.0f,  3.0f};
    glm::vec3 m_front    = {0.0f, 0.0f, -1.0f};  // direzione di sguardo
    glm::vec3 m_up       = {0.0f, 1.0f,  0.0f};

    float m_yaw   = -90.0f;  // rotazione orizzontale (sinistra-destra)
    float m_pitch =   0.0f;  // rotazione verticale   (su-giu)
    float m_speed =   5.0f;  // unita'/secondo

    float m_fov;
    float m_aspect;
    float m_near;
    float m_far;

    void updateFront(); // ricalcola m_front da yaw e pitch
};

} // namespace mini