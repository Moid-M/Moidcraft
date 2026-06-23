#pragma once
#include "core/types.hpp"
#include "block.hpp"
#include <array>
#include <glm/glm.hpp>

class Chunk {
public:
    Chunk(int cx, int cz);
    ~Chunk() = default;

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);
    bool isBlockTransparent(int x, int y, int z) const;
    bool inBounds(int x, int y, int z) const;
    bool isEmpty() const;

    glm::ivec2 position() const { return {m_cx, m_cz}; }
    u8* data() { return m_blocks.data(); }
    bool dirty() const { return m_dirty; }
    void setDirty(bool d) { m_dirty = d; }

    static int pack(int x, int y, int z) {
        return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x;
    }

private:
    int m_cx, m_cz;
    std::array<u8, CHUNK_VOLUME> m_blocks{};
    bool m_dirty = true;
};
