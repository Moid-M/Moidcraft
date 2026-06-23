#include "chunk.hpp"

Chunk::Chunk(int cx, int cz) : m_cx(cx), m_cz(cz) {
    std::fill(m_blocks.begin(), m_blocks.end(), 0);
}

BlockType Chunk::getBlock(int x, int y, int z) const {
    if (!inBounds(x, y, z)) return BlockType::Air;
    return static_cast<BlockType>(m_blocks[pack(x, y, z)]);
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (!inBounds(x, y, z)) return;
    m_blocks[pack(x, y, z)] = static_cast<u8>(type);
    m_dirty = true;
}

bool Chunk::isBlockTransparent(int x, int y, int z) const {
    auto& db = BlockDatabase::instance();
    return db.getInfo(getBlock(x, y, z)).transparent;
}

bool Chunk::inBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_HEIGHT &&
           z >= 0 && z < CHUNK_SIZE;
}

bool Chunk::isEmpty() const {
    for (auto b : m_blocks) if (b != 0) return false;
    return true;
}
