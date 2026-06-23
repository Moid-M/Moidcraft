#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void Camera::rotate(float yawDelta, float pitchDelta) {
    m_yaw += yawDelta;
    m_pitch += pitchDelta;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
}

void Camera::move(const glm::vec3& delta) {
    m_position += delta;
}

glm::mat4 Camera::viewMatrix() const {
    glm::vec3 f = forward();
    return glm::lookAt(m_position, m_position + f, glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projectionMatrix(float aspect, float fov) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);
}

glm::vec3 Camera::forward() const {
    float cy = std::cos(glm::radians(m_yaw));
    float sy = std::sin(glm::radians(m_yaw));
    float cp = std::cos(glm::radians(m_pitch));
    float sp = std::sin(glm::radians(m_pitch));
    return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}
