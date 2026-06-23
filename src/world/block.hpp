#pragma once
#include "core/types.hpp"
#include <string>
#include <unordered_map>

class TextureManager;

enum class BlockType : u8 {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Cobblestone,
    Wood,
    Leaves,
    Planks,
    Sand,
    Water,
    Glass,
    Brick,
    Bedrock,
    Snow,
    Gravel,
    CoalOre,
    IronOre,
    GoldOre,
    DiamondOre,
    NUM_BLOCK_TYPES
};

enum class BlockFaceDir : u8 {
    Front, Back, Right, Left, Top, Bottom
};

struct BlockInfo {
    BlockType type = BlockType::Air;
    bool transparent = false;
    bool liquid = false;
    bool solid = true;
    float hardness = 1.0f;
    // Minecraft texture names for each face
    std::string texTop;
    std::string texBottom;
    std::string texSide;
    std::string name = "air";

    // Resolved atlas indices (set after resource pack is loaded)
    int atlasTop = 0;
    int atlasBottom = 0;
    int atlasSide = 0;

    int textureIndex(BlockFaceDir face) const {
        switch (face) {
            case BlockFaceDir::Top: return atlasTop;
            case BlockFaceDir::Bottom: return atlasBottom;
            case BlockFaceDir::Front:
            case BlockFaceDir::Back:
            case BlockFaceDir::Right:
            case BlockFaceDir::Left: return atlasSide;
        }
        return atlasSide;
    }

    void resolveAtlasIndices(class TextureManager* texMgr);
};

class BlockDatabase {
public:
    static BlockDatabase& instance();

    const BlockInfo& getInfo(BlockType type) const;
    BlockType getType(const std::string& name) const;
    void resolveAtlasIndices(TextureManager* texMgr);

private:
    BlockDatabase();
    std::unordered_map<BlockType, BlockInfo> m_blocks;
    std::unordered_map<std::string, BlockType> m_nameMap;
};
