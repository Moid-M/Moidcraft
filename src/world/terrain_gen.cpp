#include "terrain_gen.hpp"
#include "tree_blueprint.hpp"
#include <cmath>
#include <array>
#include <algorithm>

static constexpr int SEA_LEVEL = 62;

TerrainGen::TerrainGen(int seed) : m_seed(seed),
    m_continentalNoise(seed),
    m_elevationNoise(seed + 1),
    m_detailNoise(seed + 2),
    m_temperatureNoise(seed + 3),
    m_humidityNoise(seed + 4),
    m_caveNoise(seed + 5),
    m_oreNoise(seed + 6),
    m_treeNoise(seed + 7)
{
    auto setupFbm = [](FastNoiseLite& n, float freq, int oct, float lacunarity, float gain) {
        n.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        n.SetFrequency(freq);
        n.SetFractalType(FastNoiseLite::FractalType_FBm);
        n.SetFractalOctaves(oct);
        n.SetFractalLacunarity(lacunarity);
        n.SetFractalGain(gain);
    };

    setupFbm(m_continentalNoise, 0.0015f, 4, 2.0f, 0.5f);
    setupFbm(m_elevationNoise, 0.008f, 4, 2.0f, 0.5f);
    setupFbm(m_detailNoise, 0.025f, 2, 2.0f, 0.5f);
    setupFbm(m_caveNoise, 0.03f, 3, 2.0f, 0.5f);

    m_temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_temperatureNoise.SetFrequency(0.002f);

    m_humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_humidityNoise.SetFrequency(0.002f);

    m_treeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_treeNoise.SetFrequency(0.2f);
}

BiomeType TerrainGen::getBiome(float wx, float wz, float continental) const {
    if (continental < -0.2f) return BiomeType::Ocean;

    float temp = m_temperatureNoise.GetNoise(wx, wz);
    float humid = m_humidityNoise.GetNoise(wx, wz);

    if (continental > 0.6f && temp < 0.2f)  return BiomeType::Mountains;
    if (temp > 0.35f)                        return BiomeType::Desert;
    if (temp < -0.25f)                       return BiomeType::Taiga;
    if (humid > 0.15f)                       return BiomeType::Forest;
    return BiomeType::Plains;
}

float TerrainGen::getBaseHeight(BiomeType biome, float wx, float wz) const {
    float base  = m_elevationNoise.GetNoise(wx, wz);
    float detail = m_detailNoise.GetNoise(wx, wz);

    switch (biome) {
        case BiomeType::Ocean:     return 35.0f + (base * 0.5f + 0.5f) * 18.0f;
        case BiomeType::Plains:    return 64.0f + base * 4.0f + detail * 1.0f;
        case BiomeType::Forest:    return 67.0f + base * 5.0f + detail * 2.0f;
        case BiomeType::Desert:    return 65.0f + base * 2.0f + detail * 1.0f;
        case BiomeType::Taiga:     return 67.0f + base * 7.0f + detail * 2.0f;
        case BiomeType::Mountains: return 85.0f + std::max(base, 0.0f) * 40.0f + std::abs(detail) * 5.0f;
    }
    return 64.0f;
}

void TerrainGen::generate(Chunk& chunk, Chunk* neighbors[4]) const {
    int cx = chunk.position().x;
    int cz = chunk.position().y;

    // Per-column biome and surface height
    struct ColInfo { BiomeType biome; int surfaceY; int dirtDepth; };
    std::array<ColInfo, CHUNK_SIZE * CHUNK_SIZE> cols;

    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            int wx = cx * CHUNK_SIZE + x;
            int wz = cz * CHUNK_SIZE + z;

            float continental = m_continentalNoise.GetNoise((float)wx, (float)wz);
            BiomeType b = getBiome(wx, wz, continental);
            float h = getBaseHeight(b, wx, wz);
            int sy = std::min(std::max((int)h, 1), CHUNK_HEIGHT - 1);

            float detail2d = m_detailNoise.GetNoise((float)wx, (float)wz);
            int dd = 3;
            switch (b) {
                case BiomeType::Ocean:     dd = 0; break;
                case BiomeType::Plains:    dd = 3 + (int)(detail2d * 2); break;
                case BiomeType::Forest:    dd = 4 + (int)(detail2d * 3); break;
                case BiomeType::Desert:    dd = 2; break;
                case BiomeType::Taiga:     dd = 4 + (int)(detail2d * 3); break;
                case BiomeType::Mountains: dd = 1 + (int)(std::abs(detail2d) * 2); break;
            }
            dd = std::max(dd, 0);

            cols[z * CHUNK_SIZE + x] = {b, sy, dd};
        }
    }

    // Fill blocks
    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            int wx = cx * CHUNK_SIZE + x;
            int wz = cz * CHUNK_SIZE + z;
            auto& col = cols[z * CHUNK_SIZE + x];
            int surfaceY = col.surfaceY;
            int stoneTop = surfaceY - col.dirtDepth;

            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                if (y == 0) {
                    chunk.setBlock(x, y, z, BlockType::Bedrock);
                    continue;
                }

                if (y > surfaceY) {
                    if (y <= SEA_LEVEL) chunk.setBlock(x, y, z, BlockType::Water);
                    else chunk.setBlock(x, y, z, BlockType::Air);
                    continue;
                }

                if (y == surfaceY) {
                    // Surface block
                    if (surfaceY < SEA_LEVEL + 2 && (col.biome == BiomeType::Plains || col.biome == BiomeType::Forest)) {
                        chunk.setBlock(x, y, z, BlockType::Sand);
                    } else if (col.biome == BiomeType::Desert) {
                        chunk.setBlock(x, y, z, BlockType::Sand);
                    } else if (col.biome == BiomeType::Taiga && surfaceY > 72) {
                        chunk.setBlock(x, y, z, BlockType::Snow);
                    } else if (col.biome == BiomeType::Mountains && surfaceY > 90) {
                        chunk.setBlock(x, y, z, BlockType::Snow);
                    } else if (col.biome == BiomeType::Mountains) {
                        chunk.setBlock(x, y, z, BlockType::Stone);
                    } else {
                        chunk.setBlock(x, y, z, BlockType::Grass);
                    }
                    continue;
                }

                // y < surfaceY (underground)
                if (y < stoneTop) {
                    // Deep underground: stone with caves and ores
                    float cave = m_caveNoise.GetNoise((float)wx, (float)y, (float)wz);
                    if (cave < -0.45f) {
                        chunk.setBlock(x, y, z, BlockType::Air);
                        continue;
                    }
                    float ore = m_oreNoise.GetNoise((float)wx * 0.05f, (float)y * 0.05f, (float)wz * 0.05f);
                    if (ore > 0.85f && y < 15)       chunk.setBlock(x, y, z, BlockType::DiamondOre);
                    else if (ore > 0.80f && y < 30)  chunk.setBlock(x, y, z, BlockType::GoldOre);
                    else if (ore > 0.75f && y < 60)  chunk.setBlock(x, y, z, BlockType::IronOre);
                    else if (ore > 0.70f && y < 100) chunk.setBlock(x, y, z, BlockType::CoalOre);
                    else                             chunk.setBlock(x, y, z, BlockType::Stone);
                } else {
                    // Near surface layer (between stone and surface)
                    if (col.biome == BiomeType::Desert) {
                        chunk.setBlock(x, y, z, BlockType::Sandstone);
                    } else if (col.biome == BiomeType::Mountains) {
                        chunk.setBlock(x, y, z, BlockType::Stone);
                    } else if (surfaceY < SEA_LEVEL) {
                        chunk.setBlock(x, y, z, BlockType::Sand);
                    } else {
                        chunk.setBlock(x, y, z, BlockType::Dirt);
                    }
                }
            }
        }
    }

    // Place trees
    auto posHash = [&](int wx, int wz) -> float {
        u64 h = (u64)m_seed ^ 0x9e3779b97f4a7c15ULL;
        h = h * 0xbf58476d1ce4e5b9ULL + (u64)wx;
        h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL + (u64)wz;
        h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
        h = h ^ (h >> 31);
        return (float)(h & 0x7fffffff) / (float)0x7fffffff;
    };

    auto isTooClose = [&](int x, int z, int topY) -> bool {
        for (int dx = -5; dx <= 5; dx++) {
            for (int dz = -5; dz <= 5; dz++) {
                if (dx == 0 && dz == 0) continue;
                int nx = x + dx;
                int nz = z + dz;

                Chunk* c = &chunk;
                int lx = nx, lz = nz;
                bool mappedX = false;

                if (nx < 0) {
                    if (!neighbors || !neighbors[0]) continue;
                    c = neighbors[0]; lx = nx + 16; mappedX = true;
                } else if (nx >= 16) {
                    if (!neighbors || !neighbors[1]) continue;
                    c = neighbors[1]; lx = nx - 16; mappedX = true;
                }
                if (nz < 0) {
                    if (mappedX) continue;
                    if (!neighbors || !neighbors[2]) continue;
                    c = neighbors[2]; lz = nz + 16;
                } else if (nz >= 16) {
                    if (mappedX) continue;
                    if (!neighbors || !neighbors[3]) continue;
                    c = neighbors[3]; lz = nz - 16;
                }

                for (int hy = topY - 3; hy <= topY + 10; hy++) {
                    if (hy < 0 || hy >= CHUNK_HEIGHT) continue;
                    BlockType nb = c->getBlock(lx, hy, lz);
                    if (nb == BlockType::Wood || nb == BlockType::SpruceLog)
                        return true;
                }
            }
        }
        return false;
    };

    auto placeTreeAt = [&](int x, int z, int topY, const TreeBlueprint& bp) {
        if (isTooClose(x, z, topY)) return;
        bp.place(chunk, x, topY, z, neighbors);
    };

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int wx = cx * CHUNK_SIZE + x;
            int wz = cz * CHUNK_SIZE + z;

            float cont = m_continentalNoise.GetNoise((float)wx, (float)wz);
            BiomeType biome = getBiome(wx, wz, cont);
            float h = getBaseHeight(biome, wx, wz);
            int topY = std::min(std::max((int)h, 0), CHUNK_HEIGHT - 1);
            if (topY >= CHUNK_HEIGHT - 10) continue;

            BlockType surface = (BlockType)chunk.getBlock(x, topY, z);

            if (biome == BiomeType::Forest) {
                if (surface == BlockType::Grass) {
                    float t = m_treeNoise.GetNoise((float)wx, (float)wz);
                    if (t > 0.55f) {
                        // Jitter noise and add skip chance for variety
                        float jitter = posHash(wx + 1000, wz + 2000) * 4.0f - 2.0f;
                        float jt = m_treeNoise.GetNoise((float)wx + jitter, (float)wz + jitter);
                        if (jt > 0.55f && posHash(wx, wz) > 0.15f)
                            placeTreeAt(x, z, topY, TreeBlueprint::oak());
                    }
                }
            } else if (biome == BiomeType::Plains) {
                if (surface == BlockType::Grass) {
                    float t = m_treeNoise.GetNoise((float)wx, (float)wz);
                    if (t > 0.70f) {
                        float jitter = posHash(wx + 3000, wz + 4000) * 4.0f - 2.0f;
                        float jt = m_treeNoise.GetNoise((float)wx + jitter, (float)wz + jitter);
                        if (jt > 0.70f && posHash(wx + 1, wz + 1) > 0.20f)
                            placeTreeAt(x, z, topY, TreeBlueprint::oak());
                    }
                }
            } else if (biome == BiomeType::Taiga) {
                if (surface == BlockType::Grass || surface == BlockType::Snow) {
                    float t = m_treeNoise.GetNoise((float)wx, (float)wz);
                    if (t > 0.50f) {
                        float jitter = posHash(wx + 5000, wz + 6000) * 4.0f - 2.0f;
                        float jt = m_treeNoise.GetNoise((float)wx + jitter, (float)wz + jitter);
                        if (jt > 0.50f && posHash(wx + 2, wz + 2) > 0.10f)
                            placeTreeAt(x, z, topY, TreeBlueprint::spruce());
                    }
                }
            }
        }
    }
}

void TerrainGen::generateFlat(Chunk& chunk) const {
    int cx = chunk.position().x;
    int cz = chunk.position().y;

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                if (y == 0) chunk.setBlock(x, y, z, BlockType::Bedrock);
                else if (y < 60) chunk.setBlock(x, y, z, BlockType::Stone);
                else if (y == 60) chunk.setBlock(x, y, z, BlockType::Grass);
                else chunk.setBlock(x, y, z, BlockType::Air);
            }
        }
    }
}
