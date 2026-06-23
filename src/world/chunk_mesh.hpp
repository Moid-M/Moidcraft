#pragma once
#include "core/types.hpp"
#include "vulkan/vulkan_mesh.hpp"
#include "block.hpp"
#include "chunk.hpp"
#include "core/texture_manager.hpp"
#include <vector>

class ChunkMesh {
public:
    ChunkMesh() = default;
    ~ChunkMesh() = default;

    void init(VulkanContext* ctx);
    void build(const Chunk& chunk, Chunk* neighborChunks[4], TextureManager* texMgr);
    void upload();
    bool valid() const { return m_opaqueMesh.valid() || m_transparentMesh.valid(); }
    void cleanup();

    const VulkanMesh& opaqueMesh() const { return m_opaqueMesh; }
    const VulkanMesh& transparentMesh() const { return m_transparentMesh; }

private:
    VulkanContext* m_ctx = nullptr;
    VulkanMesh m_opaqueMesh;
    VulkanMesh m_transparentMesh;
    std::vector<Vertex> m_opaqueVerts;
    std::vector<u32> m_opaqueIndices;
    std::vector<Vertex> m_transparentVerts;
    std::vector<u32> m_transparentIndices;

    void addFace(bool transparent, const glm::vec3& pos, BlockFaceDir face, const AtlasRegion& region,
                 const glm::vec3& normal, u8 light);
    void addVertex(bool transparent, const Vertex& v);
};
