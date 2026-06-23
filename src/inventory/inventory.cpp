#include "inventory.hpp"

void Inventory::init() {
    // Give starter blocks
    m_slots[0] = {BlockType::Grass, 64};
    m_slots[1] = {BlockType::Dirt, 64};
    m_slots[2] = {BlockType::Stone, 64};
    m_slots[3] = {BlockType::Cobblestone, 64};
    m_slots[4] = {BlockType::Wood, 64};
    m_slots[5] = {BlockType::Planks, 64};
    m_slots[6] = {BlockType::Glass, 64};
    m_slots[7] = {BlockType::Brick, 64};
    m_slots[8] = {BlockType::Sand, 64};
}

ItemStack& Inventory::getSlot(int index) {
    return m_slots[index];
}

const ItemStack& Inventory::getSlot(int index) const {
    return m_slots[index];
}

bool Inventory::addItem(BlockType type, int count) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (m_slots[i].type == type && m_slots[i].count < 64) {
            int space = 64 - m_slots[i].count;
            int add = std::min(space, count);
            m_slots[i].count += add;
            count -= add;
            if (count <= 0) return true;
        }
    }
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (m_slots[i].isEmpty()) {
            m_slots[i] = {type, std::min(count, 64)};
            count -= 64;
            if (count <= 0) return true;
        }
    }
    return false;
}

bool Inventory::removeItem(int slot, int count) {
    if (slot < 0 || slot >= INVENTORY_SIZE) return false;
    if (m_slots[slot].count < count) return false;
    m_slots[slot].count -= count;
    if (m_slots[slot].count <= 0) {
        m_slots[slot].type = BlockType::Air;
        m_slots[slot].count = 0;
    }
    return true;
}

void Inventory::swapSlots(int a, int b) {
    if (a < 0 || a >= INVENTORY_SIZE || b < 0 || b >= INVENTORY_SIZE) return;
    std::swap(m_slots[a], m_slots[b]);
}
