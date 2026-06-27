#pragma once
#include "chunk.hpp"
#include "chunk_mesh.hpp"
#include "terrain_gen.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_renderer.hpp"
#include "core/types.hpp"
#include "core/texture_manager.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <queue>
#include <glm/glm.hpp>

struct ChunkPairHash {
    size_t operator()(const glm::ivec2& p) const {
        return ((size_t)p.x << 32) ^ (size_t)p.y;
    }
};

class World {
public:
    World() = default;
    ~World();

    void init(VulkanContext* ctx, TextureManager* texMgr);
    void update(const glm::vec3& playerPos);
    void render(VulkanRenderer* renderer);
    void renderTransparent(VulkanRenderer* renderer);
    void renderDebug(VulkanRenderer* renderer, const glm::vec3& playerPos);
    void toggleDebugBorders() { m_showChunkBorders = !m_showChunkBorders; }
    void setRenderDistance(int dist) { m_renderDistance = std::clamp(dist, 2, 32); m_prevCx = 999999; m_prevCz = 999999; }
    int renderDistance() const { return m_renderDistance; }
    void rebuildAllMeshes();

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    // Raycasting for block interaction
    bool raycast(glm::vec3 origin, glm::vec3 dir, float maxDist,
                 glm::ivec3& hitPos, glm::ivec3& normal, BlockType& hitBlock) const;

    Chunk* getChunk(int cx, int cz);

private:
    VulkanContext* m_ctx = nullptr;
    TextureManager* m_texMgr = nullptr;
    TerrainGen m_terrainGen;

    std::unordered_map<glm::ivec2, std::unique_ptr<Chunk>, ChunkPairHash> m_chunks;
    std::unordered_map<glm::ivec2, std::unique_ptr<ChunkMesh>, ChunkPairHash> m_meshes;

    struct BuildTask {
        glm::ivec2 pos;
    };
    std::queue<BuildTask> m_buildQueue;
    std::unordered_map<glm::ivec2, bool, ChunkPairHash> m_requestedBuilds;

    int m_renderDistance = 8;
    int m_prevCx = 999999;
    int m_prevCz = 999999;

    bool m_showChunkBorders = false;
    VulkanMesh m_debugMesh;
    bool m_debugDirty = true;

    void rebuildDebugMesh(const glm::vec3& playerPos);

    Chunk* loadChunk(int cx, int cz);
    Chunk* getOrCreateChunk(int cx, int cz);
    void requestBuild(int cx, int cz);
    void rebuildMesh(int cx, int cz);
    void updateChunkLoad(const glm::vec3& playerPos);
};
