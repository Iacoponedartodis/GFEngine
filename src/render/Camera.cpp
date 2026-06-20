#include "mini/render/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace mini
{

Camera::Camera(float fovDeg, float aspect, float nearPlane, float farPlane)
    : m_fov(fovDeg), m_aspect(aspect), m_near(nearPlane), m_far(farPlane)
{ updateFront(); }

void Camera::setPosition(const glm::vec3& pos) { m_position = pos; }
void Camera::setAspect(float a)                 { m_aspect = a; }
void Camera::setSpeed(float s)                  { m_speed = s; }

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up)
{
    m_up = up;
    const glm::vec3 dir = glm::normalize(target - m_position);
    m_pitch = glm::degrees(std::asin(glm::clamp(dir.y, -1.0f, 1.0f)));
    m_yaw   = glm::degrees(std::atan2(dir.z, dir.x));
    updateFront();
}

void Camera::processKeyboard(bool fwd, bool bwd, bool lft, bool rgt,
                              bool moveUp, bool moveDown, float dt)
{
    const float dist  = m_speed * dt;
    const glm::vec3 right = glm::normalize(glm::cross(m_front, m_up));
    if (fwd)      m_position += m_front * dist;
    if (bwd)      m_position -= m_front * dist;
    if (lft)      m_position -= right   * dist;
    if (rgt)      m_position += right   * dist;
    if (moveUp)   m_position += m_up    * dist;
    if (moveDown) m_position -= m_up    * dist;
}

void Camera::processMouse(float dx, float dy, float sensitivity)
{
    m_yaw   += dx * sensitivity;
    m_pitch -= dy * sensitivity;
    m_pitch  = glm::clamp(m_pitch, -89.0f, 89.0f);
    updateFront();
}

glm::mat4 Camera::getView()           const { return glm::lookAt(m_position, m_position + m_front, m_up); }
glm::mat4 Camera::getProjection()     const { return glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far); }
glm::mat4 Camera::getViewProjection() const { return getProjection() * getView(); }

const glm::vec3& Camera::getPosition() const { return m_position; }
glm::vec3        Camera::getForward()  const { return m_front; }   // gia' normalizzato da updateFront
float            Camera::getFov()      const { return m_fov; }

void Camera::updateFront()
{
    const float yr = glm::radians(m_yaw);
    const float pr = glm::radians(m_pitch);
    m_front = glm::normalize(glm::vec3{
        std::cos(yr) * std::cos(pr),
        std::sin(pr),
        std::sin(yr) * std::cos(pr)
    });
}

} // namespace mini