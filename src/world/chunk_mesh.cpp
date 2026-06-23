#include "chunk_mesh.hpp"
#include <glm/glm.hpp>
#include <algorithm>

void ChunkMesh::init(VulkanContext* ctx) {
    m_ctx = ctx;
    m_opaqueMesh.init(ctx);
    m_transparentMesh.init(ctx);
}

void ChunkMesh::build(const Chunk& chunk, Chunk* neighborChunks[4], TextureManager* texMgr) {
    m_opaqueVerts.clear();
    m_opaqueIndices.clear();
    m_transparentVerts.clear();
    m_transparentIndices.clear();

    auto& db = BlockDatabase::instance();

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                BlockType blockType = chunk.getBlock(x, y, z);
                if (blockType == BlockType::Air) continue;

                const auto& info = db.getInfo(blockType);
                bool trans = info.transparent;

                for (int f = 0; f < 6; f++) {
                    const auto& face = BlockFaces::faces[f];
                    const auto& dir = BlockFaces::dirs[f];

                    int nx = x + dir.x;
                    int ny = y + dir.y;
                    int nz = z + dir.z;

                    BlockType neighborType = BlockType::Air;

                    if (nx < 0 && neighborChunks[0]) {
                        neighborType = neighborChunks[0]->getBlock(CHUNK_SIZE - 1, ny, nz);
                    } else if (nx >= CHUNK_SIZE && neighborChunks[1]) {
                        neighborType = neighborChunks[1]->getBlock(0, ny, nz);
                    } else if (nz < 0 && neighborChunks[2]) {
                        neighborType = neighborChunks[2]->getBlock(nx, ny, CHUNK_SIZE - 1);
                    } else if (nz >= CHUNK_SIZE && neighborChunks[3]) {
                        neighborType = neighborChunks[3]->getBlock(nx, ny, 0);
                    } else {
                        neighborType = chunk.getBlock(nx, ny, nz);
                    }

                    const auto& neighborInfo = db.getInfo(neighborType);

                    bool renderFace = false;
                    if (neighborType == BlockType::Air) renderFace = true;
                    else if (neighborInfo.transparent) renderFace = true;

                    if (renderFace) {
                        int texIdx = info.textureIndex(static_cast<BlockFaceDir>(f));
                        const auto& region = texMgr ? texMgr->getRegion(texIdx) : AtlasRegion{};

                        u8 light = 255;
                        if (face.normal.y < 0) light = 180;
                        else if (face.normal.y > 0) light = 255;
                        else if (face.normal.x != 0) light = 220;
                        else light = 210;

                        addFace(trans, glm::vec3(x, y, z), static_cast<BlockFaceDir>(f),
                                region, face.normal, light);
                    }
                }
            }
        }
    }
}

void ChunkMesh::upload() {
    m_opaqueMesh.upload(m_opaqueVerts, m_opaqueIndices);
    m_transparentMesh.upload(m_transparentVerts, m_transparentIndices);
}

void ChunkMesh::cleanup() {
    m_opaqueMesh.cleanup();
    m_transparentMesh.cleanup();
}

void ChunkMesh::addFace(bool transparent, const glm::vec3& pos, BlockFaceDir face, const AtlasRegion& region,
                        const glm::vec3& normal, u8 light) {
    auto& verts = transparent ? m_transparentVerts : m_opaqueVerts;
    auto& indices = transparent ? m_transparentIndices : m_opaqueIndices;

    const auto& faceData = BlockFaces::faces[static_cast<int>(face)];

    glm::vec2 uvs[4];
    if (region.w > 0) {
        uvs[0] = {region.u1, region.v2};
        uvs[1] = {region.u2, region.v2};
        uvs[2] = {region.u2, region.v1};
        uvs[3] = {region.u1, region.v1};
    } else {
        uvs[0] = {0, 1};
        uvs[1] = {1, 1};
        uvs[2] = {1, 0};
        uvs[3] = {0, 0};
    }

    u32 start = (u32)verts.size();
    for (int i = 0; i < 4; i++) {
        verts.push_back({pos + faceData.vertices[i], normal, uvs[i], light});
    }

    indices.push_back(start);
    indices.push_back(start + 1);
    indices.push_back(start + 2);
    indices.push_back(start);
    indices.push_back(start + 2);
    indices.push_back(start + 3);
}

void ChunkMesh::addVertex(bool transparent, const Vertex& v) {
    auto& verts = transparent ? m_transparentVerts : m_opaqueVerts;
    verts.push_back(v);
}
