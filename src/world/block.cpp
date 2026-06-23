#include "block.hpp"
#include "core/texture_manager.hpp"

BlockDatabase& BlockDatabase::instance() {
    static BlockDatabase db;
    return db;
}

BlockDatabase::BlockDatabase() {
    auto add = [&](BlockType t, const BlockInfo& info) {
        m_blocks[t] = info;
        m_nameMap[info.name] = t;
    };

    add(BlockType::Air,       {BlockType::Air, true, false, false, 0, "", "", "", "air"});
    add(BlockType::Grass,     {BlockType::Grass, false, false, true, 0.6f, "grass_block_top", "dirt", "grass_block_side", "grass"});
    add(BlockType::Dirt,      {BlockType::Dirt, false, false, true, 0.5f, "dirt", "dirt", "dirt", "dirt"});
    add(BlockType::Stone,     {BlockType::Stone, false, false, true, 1.5f, "stone", "stone", "stone", "stone"});
    add(BlockType::Cobblestone, {BlockType::Cobblestone, false, false, true, 2.0f, "cobblestone", "cobblestone", "cobblestone", "cobblestone"});
    add(BlockType::Wood,      {BlockType::Wood, false, false, true, 1.5f, "oak_log_top", "oak_log_top", "oak_log", "wood"});
    add(BlockType::Leaves,    {BlockType::Leaves, true, false, true, 0.2f, "oak_leaves", "oak_leaves", "oak_leaves", "leaves"});
    add(BlockType::Planks,    {BlockType::Planks, false, false, true, 1.0f, "oak_planks", "oak_planks", "oak_planks", "planks"});
    add(BlockType::Sand,      {BlockType::Sand, false, false, true, 0.5f, "sand", "sand", "sand", "sand"});
    add(BlockType::Water,     {BlockType::Water, true, true, false, 1.0f, "water_still", "water_still", "water_still", "water"});
    add(BlockType::Glass,     {BlockType::Glass, true, false, true, 0.3f, "glass", "glass", "glass", "glass"});
    add(BlockType::Brick,     {BlockType::Brick, false, false, true, 2.0f, "bricks", "bricks", "bricks", "brick"});
    add(BlockType::Bedrock,   {BlockType::Bedrock, false, false, true, -1.0f, "bedrock", "bedrock", "bedrock", "bedrock"});
    add(BlockType::Snow,      {BlockType::Snow, false, false, true, 0.2f, "snow", "snow", "snow", "snow"});
    add(BlockType::Gravel,    {BlockType::Gravel, false, false, true, 0.6f, "gravel", "gravel", "gravel", "gravel"});
    add(BlockType::CoalOre,   {BlockType::CoalOre, false, false, true, 3.0f, "coal_ore", "coal_ore", "coal_ore", "coal_ore"});
    add(BlockType::IronOre,   {BlockType::IronOre, false, false, true, 3.0f, "iron_ore", "iron_ore", "iron_ore", "iron_ore"});
    add(BlockType::GoldOre,   {BlockType::GoldOre, false, false, true, 3.0f, "gold_ore", "gold_ore", "gold_ore", "gold_ore"});
    add(BlockType::DiamondOre, {BlockType::DiamondOre, false, false, true, 3.0f, "diamond_ore", "diamond_ore", "diamond_ore", "diamond_ore"});
}

const BlockInfo& BlockDatabase::getInfo(BlockType type) const {
    static BlockInfo air{BlockType::Air, true, false, false, 0, "", "", "", "air"};
    auto it = m_blocks.find(type);
    return it != m_blocks.end() ? it->second : air;
}

BlockType BlockDatabase::getType(const std::string& name) const {
    auto it = m_nameMap.find(name);
    return it != m_nameMap.end() ? it->second : BlockType::Air;
}

void BlockDatabase::resolveAtlasIndices(TextureManager* texMgr) {
    for (auto& [type, info] : m_blocks) {
        info.atlasTop = texMgr->getTextureIndex(info.texTop);
        info.atlasBottom = texMgr->getTextureIndex(info.texBottom);
        info.atlasSide = texMgr->getTextureIndex(info.texSide);

        if (info.atlasTop < 0) info.atlasTop = 0;
        if (info.atlasBottom < 0) info.atlasBottom = 0;
        if (info.atlasSide < 0) info.atlasSide = 0;
    }
}

void BlockInfo::resolveAtlasIndices(TextureManager* texMgr) {
    // Called per-block if needed
}
