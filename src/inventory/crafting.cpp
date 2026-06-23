#include "crafting.hpp"

CraftingTable& CraftingTable::instance() {
    static CraftingTable table;
    return table;
}

CraftingTable::CraftingTable() {
    m_recipes = {
        {{{BlockType::Wood, 1}}, BlockType::Planks, 4, "Planks"},
        {{{BlockType::Planks, 4}}, BlockType::Cobblestone, 1, "Crafting Table"}, // placeholder
        {{{BlockType::Cobblestone, 8}}, BlockType::Stone, 1, "Stone"}, // smelting placeholder
        {{{BlockType::Sand, 4}}, BlockType::Glass, 1, "Glass"},
        {{{BlockType::Stone, 4}}, BlockType::Brick, 4, "Bricks"},
    };
}

bool CraftingTable::canCraft(Inventory& inv, const CraftingRecipe& recipe) const {
    for (auto& input : recipe.inputs) {
        int needed = input.second;
        for (int i = 0; i < Inventory::INVENTORY_SIZE; i++) {
            auto& slot = inv.getSlot(i);
            if (slot.type == input.first) {
                needed -= slot.count;
                if (needed <= 0) break;
            }
        }
        if (needed > 0) return false;
    }
    return true;
}

bool CraftingTable::craft(Inventory& inv, int recipeIndex) {
    if (recipeIndex < 0 || recipeIndex >= (int)m_recipes.size()) return false;
    auto& recipe = m_recipes[recipeIndex];

    if (!canCraft(inv, recipe)) return false;

    // Remove inputs
    for (auto& input : recipe.inputs) {
        int toRemove = input.second;
        for (int i = 0; i < Inventory::INVENTORY_SIZE && toRemove > 0; i++) {
            auto& slot = inv.getSlot(i);
            if (slot.type == input.first) {
                int remove = std::min(slot.count, toRemove);
                slot.count -= remove;
                toRemove -= remove;
                if (slot.count <= 0) {
                    slot.type = BlockType::Air;
                }
            }
        }
    }

    // Add output
    inv.addItem(recipe.output, recipe.outputCount);
    return true;
}
