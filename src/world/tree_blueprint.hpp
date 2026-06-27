#pragma once
#include "block.hpp"
#include "chunk.hpp"
#include <vector>
#include <string>

struct BlueprintBlock {
    int dx, dy, dz;
    BlockType type;
};

class TreeBlueprint {
public:
    std::string name;
    std::vector<BlueprintBlock> blocks;

    void place(Chunk& chunk, int trunkX, int surfaceY, int trunkZ, Chunk* neighbors[4] = nullptr) const;

    static const TreeBlueprint& oak();
    static const TreeBlueprint& spruce();
};
