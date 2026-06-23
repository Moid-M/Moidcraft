#pragma once
#include "core/types.hpp"
#include "core/camera.hpp"
#include "core/input.hpp"
#include "world/world.hpp"
#include "inventory/inventory.hpp"
#include <glm/glm.hpp>

class Player {
public:
    Player();
    ~Player() = default;

    void init(Camera* camera, Input* input, World* world);
    void update(float dt);

    Camera* camera() { return m_camera; }
    Inventory& inventory() { return m_inventory; }
    glm::vec3 position() const { return m_camera->position(); }

    int selectedSlot() const { return m_selectedSlot; }
    void setSelectedSlot(int slot);

    void setFlying(bool f) { m_flying = f; }
    bool flying() const { return m_flying; }

private:
    Camera* m_camera = nullptr;
    Input* m_input = nullptr;
    World* m_world = nullptr;
    Inventory m_inventory;

    glm::vec3 m_position{0, 70, 0};
    glm::vec3 m_velocity{0};
    bool m_onGround = false;
    bool m_flying = false;
    int m_selectedSlot = 0;

    float m_mouseSensitivity = 0.1f;
    float m_moveSpeed = 5.0f;
    float m_jumpPower = 8.0f;
    float m_gravity = -25.0f;

    static constexpr float PLAYER_WIDTH = 0.6f;
    static constexpr float PLAYER_HEIGHT = 1.8f;
    static constexpr float EYE_HEIGHT = 1.6f;

    void handleInput(float dt);
    void handleMovement(float dt);
    void handleInteraction();

    bool collidesAt(const glm::vec3& feetPos) const;
    glm::vec3 moveOnAxis(const glm::vec3& from, float amount, int axis) const;
};
