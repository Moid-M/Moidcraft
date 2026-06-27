#include "world.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

World::~World() {
    m_meshes.clear();
    m_chunks.clear();
}

void World::init(VulkanContext* ctx, TextureManager* texMgr) {
    m_ctx = ctx;
    m_texMgr = texMgr;
    m_debugMesh.init(ctx);
}

Chunk* World::getChunk(int cx, int cz) {
    auto it = m_chunks.find({cx, cz});
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

Chunk* World::getOrCreateChunk(int cx, int cz) {
    auto key = glm::ivec2(cx, cz);
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) return it->second.get();

    auto chunk = std::make_unique<Chunk>(cx, cz);
    auto* ptr = chunk.get();

    Chunk* neighbors[4] = {};
    {
        auto n = m_chunks.find({cx - 1, cz});
        if (n != m_chunks.end()) neighbors[0] = n->second.get();
    }
    {
        auto n = m_chunks.find({cx + 1, cz});
        if (n != m_chunks.end()) neighbors[1] = n->second.get();
    }
    {
        auto n = m_chunks.find({cx, cz - 1});
        if (n != m_chunks.end()) neighbors[2] = n->second.get();
    }
    {
        auto n = m_chunks.find({cx, cz + 1});
        if (n != m_chunks.end()) neighbors[3] = n->second.get();
    }

    m_terrainGen.generate(*ptr, neighbors);
    m_chunks[key] = std::move(chunk);
    return ptr;
}

void World::requestBuild(int cx, int cz) {
    glm::ivec2 key(cx, cz);
    if (m_requestedBuilds.find(key) != m_requestedBuilds.end()) return;
    m_requestedBuilds[key] = true;
    m_buildQueue.push({key});
}

void World::rebuildMesh(int cx, int cz) {
    Chunk* chunk = getChunk(cx, cz);
    if (!chunk) return;

    Chunk* neighbors[4] = {
        getChunk(cx - 1, cz),
        getChunk(cx + 1, cz),
        getChunk(cx, cz - 1),
        getChunk(cx, cz + 1)
    };

    auto key = glm::ivec2(cx, cz);
    auto meshIt = m_meshes.find(key);
    if (meshIt == m_meshes.end()) {
        auto mesh = std::make_unique<ChunkMesh>();
        mesh->init(m_ctx);
        meshIt = m_meshes.emplace(key, std::move(mesh)).first;
    }

    meshIt->second->build(*chunk, neighbors, m_texMgr);
    meshIt->second->upload();
    chunk->setDirty(false);

    glm::ivec2 k(key);
    m_requestedBuilds.erase(k);
}

void World::updateChunkLoad(const glm::vec3& playerPos) {
    int pcx = (int)std::floor(playerPos.x / CHUNK_SIZE);
    int pcz = (int)std::floor(playerPos.z / CHUNK_SIZE);

    if (pcx == m_prevCx && pcz == m_prevCz) {
        int buildsThisFrame = 0;
        int maxBuilds = 4;
        while (!m_buildQueue.empty() && buildsThisFrame < maxBuilds) {
            auto task = m_buildQueue.front();
            m_buildQueue.pop();
            if (getChunk(task.pos.x, task.pos.y)) {
                rebuildMesh(task.pos.x, task.pos.y);
                buildsThisFrame++;
            }
        }
        return;
    }

    m_prevCx = pcx;
    m_prevCz = pcz;
    m_debugDirty = true;

    int radius = m_renderDistance;
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            int cx = pcx + dx;
            int cz = pcz + dz;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > radius) continue;

            getOrCreateChunk(cx, cz);
            requestBuild(cx, cz);
        }
    }

    std::vector<glm::ivec2> toRemove;
    for (auto& [pos, chunk] : m_chunks) {
        int dx = pos.x - pcx;
        int dz = pos.y - pcz;
        if (dx * dx + dz * dz > (radius + 4) * (radius + 4)) {
            toRemove.push_back(pos);
        }
    }
    for (auto& pos : toRemove) {
        m_meshes.erase(pos);
        m_chunks.erase(pos);
    }
}

void World::update(const glm::vec3& playerPos) {
    updateChunkLoad(playerPos);
}

void World::render(VulkanRenderer* renderer) {
    for (auto& [pos, mesh] : m_meshes) {
        if (mesh->opaqueMesh().valid()) {
            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(pos.x * CHUNK_SIZE, 0, pos.y * CHUNK_SIZE));
            renderer->drawMesh(mesh->opaqueMesh(), model);
        }
    }
}

void World::renderTransparent(VulkanRenderer* renderer) {
    for (auto& [pos, mesh] : m_meshes) {
        if (mesh->transparentMesh().valid()) {
            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(pos.x * CHUNK_SIZE, 0, pos.y * CHUNK_SIZE));
            renderer->drawMeshTransparent(mesh->transparentMesh(), model);
        }
    }
}

void World::rebuildAllMeshes() {
    for (auto& [pos, chunk] : m_chunks) {
        rebuildMesh(pos.x, pos.y);
    }
}

void World::rebuildDebugMesh(const glm::vec3& playerPos) {
    if (m_chunks.empty()) return;

    int pcx = (int)std::floor(playerPos.x / CHUNK_SIZE);
    int pcz = (int)std::floor(playerPos.z / CHUNK_SIZE);

    int texIdx = m_texMgr->getTextureIndex("debug_border");
    const auto& region = m_texMgr->getRegion(texIdx);
    glm::vec2 uv((region.u1 + region.u2) * 0.5f, (region.v1 + region.v2) * 0.5f);
    Vertex vertTemplate{};
    vertTemplate.normal = glm::vec3(0.0f);
    vertTemplate.uv = uv;
    vertTemplate.lighting = 255;

    std::vector<Vertex> verts;
    verts.reserve(m_chunks.size() * 24);

    for (auto& [pos, chunk] : m_chunks) {
        float x1 = (float)(pos.x * CHUNK_SIZE);
        float x2 = x1 + CHUNK_SIZE;
        float z1 = (float)(pos.y * CHUNK_SIZE);
        float z2 = z1 + CHUNK_SIZE;
        float y1 = 0.0f;
        float y2 = (float)CHUNK_HEIGHT;
        bool isCurrentChunk = (pos.x == pcx && pos.y == pcz);

        auto addVert = [&](float x, float y, float z) {
            vertTemplate.pos = glm::vec3(x, y, z);
            verts.push_back(vertTemplate);
        };

        auto addLine = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
            addVert(x0, y0, z0);
            addVert(x1, y1, z1);
        };

        // Bottom face (y=0) — always drawn
        addLine(x1, y1, z1, x2, y1, z1);
        addLine(x2, y1, z1, x2, y1, z2);
        addLine(x2, y1, z2, x1, y1, z2);
        addLine(x1, y1, z2, x1, y1, z1);

        if (isCurrentChunk) {
            // Top face (y=256)
            addLine(x1, y2, z1, x2, y2, z1);
            addLine(x2, y2, z1, x2, y2, z2);
            addLine(x2, y2, z2, x1, y2, z2);
            addLine(x1, y2, z2, x1, y2, z1);
            // Vertical edges
            addLine(x1, y1, z1, x1, y2, z1);
            addLine(x2, y1, z1, x2, y2, z1);
            addLine(x2, y1, z2, x2, y2, z2);
            addLine(x1, y1, z2, x1, y2, z2);
        }
    }

    m_debugMesh.upload(verts, {});
}

void World::renderDebug(VulkanRenderer* renderer, const glm::vec3& playerPos) {
    if (!m_showChunkBorders) return;
    if (m_debugDirty) {
        rebuildDebugMesh(playerPos);
        m_debugDirty = false;
    }
    if (m_debugMesh.valid()) {
        renderer->drawMeshDebug(m_debugMesh, glm::mat4(1.0f));
    }
}

BlockType World::getBlock(int x, int y, int z) const {
    int cx = (int)std::floor((float)x / CHUNK_SIZE);
    int cz = (int)std::floor((float)z / CHUNK_SIZE);
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;

    auto it = m_chunks.find({cx, cz});
    if (it == m_chunks.end()) return BlockType::Air;
    return it->second->getBlock(lx, y, lz);
}

void World::setBlock(int x, int y, int z, BlockType type) {
    int cx = (int)std::floor((float)x / CHUNK_SIZE);
    int cz = (int)std::floor((float)z / CHUNK_SIZE);
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;

    auto it = m_chunks.find({cx, cz});
    if (it == m_chunks.end()) return;
    it->second->setBlock(lx, y, lz, type);
    rebuildMesh(cx, cz);

    if (lx == 0) rebuildMesh(cx - 1, cz);
    if (lx == CHUNK_SIZE - 1) rebuildMesh(cx + 1, cz);
    if (lz == 0) rebuildMesh(cx, cz - 1);
    if (lz == CHUNK_SIZE - 1) rebuildMesh(cx, cz + 1);
}

bool World::raycast(glm::vec3 origin, glm::vec3 dir, float maxDist,
                    glm::ivec3& hitPos, glm::ivec3& normal, BlockType& hitBlock) const {
    dir = glm::normalize(dir);

    int bx = (int)std::floor(origin.x);
    int by = (int)std::floor(origin.y);
    int bz = (int)std::floor(origin.z);

    int stepX = dir.x > 0 ? 1 : -1;
    int stepY = dir.y > 0 ? 1 : -1;
    int stepZ = dir.z > 0 ? 1 : -1;

    float tDeltaX = (dir.x != 0) ? std::abs(1.0f / dir.x) : std::numeric_limits<float>::max();
    float tDeltaY = (dir.y != 0) ? std::abs(1.0f / dir.y) : std::numeric_limits<float>::max();
    float tDeltaZ = (dir.z != 0) ? std::abs(1.0f / dir.z) : std::numeric_limits<float>::max();

    float tMaxX = (dir.x != 0) ? ((dir.x > 0 ? (bx + 1) : bx) - origin.x) / dir.x : std::numeric_limits<float>::max();
    float tMaxY = (dir.y != 0) ? ((dir.y > 0 ? (by + 1) : by) - origin.y) / dir.y : std::numeric_limits<float>::max();
    float tMaxZ = (dir.z != 0) ? ((dir.z > 0 ? (bz + 1) : bz) - origin.z) / dir.z : std::numeric_limits<float>::max();

    int faceX = 0, faceY = 0, faceZ = 0;
    float t = 0.0f;

    for (int i = 0; i < 200 && t <= maxDist; i++) {
        BlockType block = getBlock(bx, by, bz);
        if (block != BlockType::Air && block != BlockType::Water) {
            hitPos = glm::ivec3(bx, by, bz);
            hitBlock = block;
            normal = glm::ivec3(faceX, faceY, faceZ);
            return true;
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                bx += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                faceX = -stepX; faceY = 0; faceZ = 0;
            } else {
                bz += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                faceX = 0; faceY = 0; faceZ = -stepZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                by += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                faceX = 0; faceY = -stepY; faceZ = 0;
            } else {
                bz += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                faceX = 0; faceY = 0; faceZ = -stepZ;
            }
        }
    }

    return false;
}
