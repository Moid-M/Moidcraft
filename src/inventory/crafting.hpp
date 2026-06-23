#pragma once
#include "inventory.hpp"
#include <vector>
#include <functional>

struct CraftingRecipe {
    std::vector<std::pair<BlockType, int>> inputs;
    BlockType output;
    int outputCount = 1;
    std::string name;
};

class CraftingTable {
public:
    static CraftingTable& instance();

    bool canCraft(Inventory& inv, const CraftingRecipe& recipe) const;
    bool craft(Inventory& inv, int recipeIndex);
    const std::vector<CraftingRecipe>& recipes() const { return m_recipes; }

private:
    CraftingTable();
    std::vector<CraftingRecipe> m_recipes;
};
