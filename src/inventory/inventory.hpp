#pragma once
#include "core/types.hpp"
#include "world/block.hpp"
#include <array>

struct ItemStack {
    BlockType type = BlockType::Air;
    int count = 0;

    bool isEmpty() const { return type == BlockType::Air || count <= 0; }
};

class Inventory {
public:
    void init();

    ItemStack& getSlot(int index);
    const ItemStack& getSlot(int index) const;

    bool addItem(BlockType type, int count = 1);
    bool removeItem(int slot, int count = 1);
    void swapSlots(int a, int b);

    static constexpr int HOTBAR_SIZE = 9;
    static constexpr int INVENTORY_SIZE = 36;

private:
    std::array<ItemStack, INVENTORY_SIZE> m_slots{};
};
