#include "player.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

Player::Player() {
    m_inventory.init();
}

void Player::init(Camera* camera, Input* input, World* world) {
    m_camera = camera;
    m_input = input;
    m_world = world;
    m_position = glm::vec3(0, 70, 0);
    m_camera->setPosition(m_position + glm::vec3(0, EYE_HEIGHT, 0));
}

void Player::update(float dt) {
    handleInput(dt);
    handleMovement(dt);
    handleInteraction();
    m_camera->setPosition(m_position + glm::vec3(0, EYE_HEIGHT, 0));
}

void Player::handleInput(float) {
    if (!m_input) return;

    glm::vec2 delta = m_input->mouseDelta();
    m_camera->rotate(delta.x * m_mouseSensitivity, -delta.y * m_mouseSensitivity);

    if (m_input->keyPressed(GLFW_KEY_F)) {
        m_flying = !m_flying;
    }

    for (int i = 0; i <= 8; i++) {
        if (m_input->keyPressed(GLFW_KEY_1 + i)) {
            m_selectedSlot = i;
        }
    }
    double scroll = m_input->scrollDelta();
    if (scroll > 0) {
        m_selectedSlot = (m_selectedSlot + 1) % 9;
    } else if (scroll < 0) {
        m_selectedSlot = (m_selectedSlot - 1 + 9) % 9;
    }
}

bool Player::collidesAt(const glm::vec3& feetPos) const {
    if (!m_world) return false;

    float r = PLAYER_WIDTH * 0.5f;
    float minX = feetPos.x - r, maxX = feetPos.x + r;
    float minY = feetPos.y,     maxY = feetPos.y + PLAYER_HEIGHT;
    float minZ = feetPos.z - r, maxZ = feetPos.z + r;

    int bx0 = (int)std::floor(minX);
    int bx1 = (int)std::floor(maxX);
    int by0 = (int)std::floor(minY);
    int by1 = (int)std::floor(maxY);
    int bz0 = (int)std::floor(minZ);
    int bz1 = (int)std::floor(maxZ);

    for (int bx = bx0; bx <= bx1; bx++) {
        for (int by = by0; by <= by1; by++) {
            for (int bz = bz0; bz <= bz1; bz++) {
                BlockType block = m_world->getBlock(bx, by, bz);
                if (block == BlockType::Air) continue;
                auto& info = BlockDatabase::instance().getInfo(block);
                if (!info.solid) continue;

                if (minX < bx + 1 && maxX > bx &&
                    minY < by + 1 && maxY > by &&
                    minZ < bz + 1 && maxZ > bz) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool Player::isInWater() const {
    if (!m_world) return false;
    float r = PLAYER_WIDTH * 0.5f;
    int bx0 = (int)std::floor(m_position.x - r);
    int bx1 = (int)std::floor(m_position.x + r);
    int by0 = (int)std::floor(m_position.y);
    int by1 = (int)std::floor(m_position.y + PLAYER_HEIGHT);
    int bz0 = (int)std::floor(m_position.z - r);
    int bz1 = (int)std::floor(m_position.z + r);
    for (int bx = bx0; bx <= bx1; bx++)
        for (int by = by0; by <= by1; by++)
            for (int bz = bz0; bz <= bz1; bz++)
                if (m_world->getBlock(bx, by, bz) == BlockType::Water)
                    return true;
    return false;
}

glm::vec3 Player::moveOnAxis(const glm::vec3& from, float amount, int axis) const {
    if (amount == 0.0f) return from;

    glm::vec3 step = from;
    step[axis] += amount;

    if (!collidesAt(step)) return step;

    // Snap to nearest block boundary on this axis
    float r = PLAYER_WIDTH * 0.5f;
    if (axis == 0) { // X
        if (amount > 0) step.x = std::floor(step.x + r) - r;
        else            step.x = std::ceil(step.x - r) + r;
    } else if (axis == 1) { // Y
        if (amount > 0) step.y = std::floor(step.y + PLAYER_HEIGHT);
        else            step.y = std::ceil(step.y);
    } else { // Z
        if (amount > 0) step.z = std::floor(step.z + r) - r;
        else            step.z = std::ceil(step.z - r) + r;
    }
    if (!collidesAt(step)) return step;

    // If still colliding, do finer stepping
    float stepSize = 0.05f;
    step = from;
    float remaining = amount;
    while (std::abs(remaining) > stepSize) {
        float sub = (remaining > 0) ? stepSize : -stepSize;
        step[axis] += sub;
        if (collidesAt(step)) {
            step[axis] -= sub;
            break;
        }
        remaining -= sub;
    }
    return step;
}

void Player::handleMovement(float dt) {
    if (!m_input || !m_camera) return;
    if (dt > 0.1f) dt = 0.1f;

    glm::vec3 forward = m_camera->forward();
    glm::vec3 right = m_camera->right();
    glm::vec3 wishDir(0);

    if (m_input->keyDown(GLFW_KEY_W)) wishDir += forward;
    if (m_input->keyDown(GLFW_KEY_S)) wishDir -= forward;
    if (m_input->keyDown(GLFW_KEY_A)) wishDir -= right;
    if (m_input->keyDown(GLFW_KEY_D)) wishDir += right;

    wishDir.y = 0;
    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir) * m_moveSpeed;
    }

    if (m_flying) {
        if (m_input->keyDown(GLFW_KEY_SPACE)) wishDir.y = m_moveSpeed;
        if (m_input->keyDown(GLFW_KEY_LEFT_SHIFT)) wishDir.y = -m_moveSpeed;
        m_velocity = wishDir;
        m_position += m_velocity * dt;
        return;
    }

    // Walking
    m_velocity.x = wishDir.x;
    m_velocity.z = wishDir.z;

    bool inWater = isInWater();

    if (inWater) {
        if (m_input->keyDown(GLFW_KEY_SPACE))
            m_velocity.y = 4.0f;
        else if (m_input->keyDown(GLFW_KEY_LEFT_SHIFT))
            m_velocity.y = -2.0f;
        else
            m_velocity.y += m_gravity * 0.15f * dt;
        m_velocity.x *= 0.85f;
        m_velocity.z *= 0.85f;
        m_onGround = false;
    } else {
        if (m_input->keyDown(GLFW_KEY_SPACE) && m_onGround) {
            m_velocity.y = m_jumpPower;
            m_onGround = false;
        }
        m_velocity.y += m_gravity * dt;
    }

    // Move step by step along each axis, checking collision per axis
    glm::vec3 newPos = m_position;

    newPos = moveOnAxis(newPos, m_velocity.x * dt, 0);
    newPos = moveOnAxis(newPos, m_velocity.y * dt, 1);
    newPos = moveOnAxis(newPos, m_velocity.z * dt, 2);

    if (!inWater) {
        if (m_velocity.y <= 0.0f) {
            glm::vec3 belowCheck = newPos;
            belowCheck.y -= 0.05f;
            m_onGround = collidesAt(belowCheck);
            if (m_onGround) m_velocity.y = 0.0f;
        } else {
            m_onGround = false;
        }
    }

    if (collidesAt(newPos)) {
        if (collidesAt({newPos.x, m_position.y, newPos.z})) {
            m_velocity.x = 0;
            m_velocity.z = 0;
        }
    }

    m_position = newPos;

    if (m_position.y < -10.0f) {
        m_position.y = 70.0f;
        m_velocity = glm::vec3(0);
    }
}

void Player::handleInteraction() {
    if (!m_input || !m_world) return;

    glm::vec3 origin = m_position + glm::vec3(0, EYE_HEIGHT, 0);
    glm::vec3 dir = m_camera->forward();
    glm::ivec3 hitPos, normal;
    BlockType hitBlock;

    if (m_input->mouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (m_world->raycast(origin, dir, 8.0f, hitPos, normal, hitBlock)) {
            m_world->setBlock(hitPos.x, hitPos.y, hitPos.z, BlockType::Air);
        }
    }

    if (m_input->mouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        if (m_world->raycast(origin, dir, 8.0f, hitPos, normal, hitBlock)) {
            glm::ivec3 placePos = hitPos + normal;
            BlockType currentBlock = m_world->getBlock(placePos.x, placePos.y, placePos.z);
            if (currentBlock == BlockType::Air) {
                auto& slot = m_inventory.getSlot(m_selectedSlot);
                if (slot.type != BlockType::Air && slot.count > 0) {
                    m_world->setBlock(placePos.x, placePos.y, placePos.z, slot.type);
                    slot.count--;
                    if (slot.count <= 0) {
                        slot.type = BlockType::Air;
                    }
                }
            }
        }
    }
}

void Player::setSelectedSlot(int slot) {
    m_selectedSlot = std::clamp(slot, 0, 8);
}
