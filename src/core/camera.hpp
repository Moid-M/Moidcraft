#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera() = default;

    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void setRotation(float yaw, float pitch) { m_yaw = yaw; m_pitch = pitch; }

    void rotate(float yawDelta, float pitchDelta);
    void move(const glm::vec3& delta);

    const glm::vec3& position() const { return m_position; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect, float fov = 75.0f) const;
    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

private:
    glm::vec3 m_position{0, 60, 0};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};
