#include "tree_blueprint.hpp"

void TreeBlueprint::place(Chunk& chunk, int trunkX, int surfaceY, int trunkZ, Chunk* neighbors[4]) const {
    for (auto& b : blocks) {
        int bx = trunkX + b.dx;
        int by = surfaceY + b.dy;
        int bz = trunkZ + b.dz;
        if (by < 0 || by >= 256) continue;

        Chunk* target = &chunk;
        int lx = bx, lz = bz;

        if (bx < 0) {
            if (neighbors && neighbors[0]) { target = neighbors[0]; lx = bx + 16; }
            else continue;
        } else if (bx >= 16) {
            if (neighbors && neighbors[1]) { target = neighbors[1]; lx = bx - 16; }
            else continue;
        }
        if (bz < 0) {
            if (neighbors && neighbors[2]) { target = neighbors[2]; lz = bz + 16; }
            else continue;
        } else if (bz >= 16) {
            if (neighbors && neighbors[3]) { target = neighbors[3]; lz = bz - 16; }
            else continue;
        }

        if (b.type == BlockType::Leaves || b.type == BlockType::SpruceLeaves) {
            if (target->getBlock(lx, by, lz) == BlockType::Air)
                target->setBlock(lx, by, lz, b.type);
        } else {
            target->setBlock(lx, by, lz, b.type);
        }
    }
}

struct BlueprintData {
    std::vector<BlueprintBlock> oakBlocks;
    std::vector<BlueprintBlock> spruceBlocks;
};

static const BlueprintData& buildData() {
    static BlueprintData d;
    if (!d.oakBlocks.empty()) return d;

    auto addLeaf = [&](std::vector<BlueprintBlock>& vec, int dx, int dy, int dz, BlockType type) {
        vec.push_back({dx, dy, dz, type});
    };

    // ========== OAK TREE (Minecraft small oak pattern) ==========
    // Trunk: 5 logs at dy=1..5 (first block is 1 above surface)
    for (int i = 1; i <= 5; i++)
        d.oakBlocks.push_back({0, i, 0, BlockType::Wood});

    // dy=3 (3 above surface): 5x5 minus corners (20 leaves, center is trunk)
    for (int x = -2; x <= 2; x++)
        for (int z = -2; z <= 2; z++) {
            if (std::abs(x) == 2 && std::abs(z) == 2) continue;
            if (x == 0 && z == 0) continue;
            addLeaf(d.oakBlocks, x, 3, z, BlockType::Leaves);
        }

    // dy=4: 5x5 minus corners (20 leaves, center is trunk)
    for (int x = -2; x <= 2; x++)
        for (int z = -2; z <= 2; z++) {
            if (std::abs(x) == 2 && std::abs(z) == 2) continue;
            if (x == 0 && z == 0) continue;
            addLeaf(d.oakBlocks, x, 4, z, BlockType::Leaves);
        }

    // dy=5 (trunk top): 3x3 ring (8 leaves, center is trunk)
    for (int x = -1; x <= 1; x++)
        for (int z = -1; z <= 1; z++) {
            if (x == 0 && z == 0) continue;
            addLeaf(d.oakBlocks, x, 5, z, BlockType::Leaves);
        }

    // dy=6 (above trunk): + cross shape (5 leaves)
    addLeaf(d.oakBlocks, -1, 6, 0, BlockType::Leaves);
    addLeaf(d.oakBlocks, 0, 6, -1, BlockType::Leaves);
    addLeaf(d.oakBlocks, 0, 6, 0, BlockType::Leaves);
    addLeaf(d.oakBlocks, 0, 6, 1, BlockType::Leaves);
    addLeaf(d.oakBlocks, 1, 6, 0, BlockType::Leaves);

    // ========== SPRUCE TREE (Minecraft Normal pattern, 97 leaves, bottom-to-top) ==========
    // Trunk: 7 logs at dy=1..7 (dy=1 is 1 block above surface)
    for (int i = 1; i <= 7; i++)
        d.spruceBlocks.push_back({0, i, 0, BlockType::SpruceLog});

    // dy=1: Layer 1 — trunk only (no leaves at first trunk block)

    // dy=2: Layer 2 — 7x7 ring, corners excluded (44 leaves + trunk)
    for (int x = -3; x <= 3; x++)
        for (int z = -3; z <= 3; z++) {
            if (std::abs(x) == 3 && std::abs(z) == 3) continue;
            if (x == 0 && z == 0) continue;
            addLeaf(d.spruceBlocks, x, 2, z, BlockType::SpruceLeaves);
        }

    // dy=3: Layer 3 — 5x5 diamond (20 leaves + trunk)
    for (int x = -2; x <= 2; x++)
        for (int z = -2; z <= 2; z++) {
            if (std::abs(x) == 2 && std::abs(z) == 2) continue;
            if (x == 0 && z == 0) continue;
            addLeaf(d.spruceBlocks, x, 3, z, BlockType::SpruceLeaves);
        }

    // dy=4: Layer 4 — 3x3 cross (4 leaves + trunk)
    addLeaf(d.spruceBlocks, -1, 4, 0, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 4, -1, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 4, 1, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 1, 4, 0, BlockType::SpruceLeaves);

    // dy=5: Layer 5 — 5x5 diamond (20 leaves + trunk) — same as Layer 3
    for (int x = -2; x <= 2; x++)
        for (int z = -2; z <= 2; z++) {
            if (std::abs(x) == 2 && std::abs(z) == 2) continue;
            if (x == 0 && z == 0) continue;
            addLeaf(d.spruceBlocks, x, 5, z, BlockType::SpruceLeaves);
        }

    // dy=6: Layer 6 — 3x3 cross (4 leaves + trunk) — same as Layer 4
    addLeaf(d.spruceBlocks, -1, 6, 0, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 6, -1, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 6, 1, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 1, 6, 0, BlockType::SpruceLeaves);

    // dy=7: Layer 7 — trunk only (no leaves at top trunk block)

    // dy=8: Layer 8 — 3x3 cross with center leaf (5 leaves, NO trunk)
    addLeaf(d.spruceBlocks, -1, 8, 0, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 8, -1, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 8, 0, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 0, 8, 1, BlockType::SpruceLeaves);
    addLeaf(d.spruceBlocks, 1, 8, 0, BlockType::SpruceLeaves);

    // dy=9: single tip leaf at the very top
    d.spruceBlocks.push_back({0, 9, 0, BlockType::SpruceLeaves});

    return d;
}

const TreeBlueprint& TreeBlueprint::oak() {
    static TreeBlueprint bp;
    if (bp.blocks.empty()) {
        bp.name = "oak";
        bp.blocks = buildData().oakBlocks;
    }
    return bp;
}

const TreeBlueprint& TreeBlueprint::spruce() {
    static TreeBlueprint bp;
    if (bp.blocks.empty()) {
        bp.name = "spruce";
        bp.blocks = buildData().spruceBlocks;
    }
    return bp;
}
