#include "mini/render/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace mini
{

Camera::Camera(float fovDeg, float aspect, float nearPlane, float farPlane)
    : m_fov(fovDeg)
    , m_aspect(aspect)
    , m_near(nearPlane)
    , m_far(farPlane)
{
}

void Camera::setPosition(const glm::vec3& pos)
{
    m_position = pos;
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up)
{
    m_target = target;
    m_up     = up;
}

void Camera::setAspect(float aspect)
{
    m_aspect = aspect;
}

glm::mat4 Camera::getView() const
{
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::getProjection() const
{
    return glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
}

glm::mat4 Camera::getViewProjection() const
{
    return getProjection() * getView();
}

const glm::vec3& Camera::getPosition() const
{
    return m_position;
}

float Camera::getFov() const
{
    return m_fov;
}

} // namespace mini