#include "terrain_gen.hpp"
#include <cmath>
#include <array>

TerrainGen::TerrainGen(int seed) : m_seed(seed),
    m_heightNoise(seed),
    m_densityNoise(seed),
    m_caveNoise(seed + 1),
    m_oreNoise(seed + 2),
    m_treeNoise(seed + 3)
{
    m_heightNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_heightNoise.SetFrequency(0.01f);
    m_heightNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_heightNoise.SetFractalOctaves(4);
    m_heightNoise.SetFractalLacunarity(2.0f);
    m_heightNoise.SetFractalGain(0.5f);

    m_densityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_densityNoise.SetFrequency(0.02f);
    m_densityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_densityNoise.SetFractalOctaves(3);
    m_densityNoise.SetFractalLacunarity(2.0f);
    m_densityNoise.SetFractalGain(0.5f);

    m_caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caveNoise.SetFrequency(0.03f);
    m_caveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_caveNoise.SetFractalOctaves(3);

    m_treeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_treeNoise.SetFrequency(0.5f);
}

float TerrainGen::sampleHeight(int wx, int wz) const {
    float h = m_heightNoise.GetNoise((float)wx, (float)wz);
    return (h + 1.0f) * 40.0f + 20.0f;
}

float TerrainGen::sampleDensity(int wx, int wy, int wz) const {
    float heightMap = sampleHeight(wx, wz);
    float val = 0.0f;

    if (wy < heightMap - 4) val = 1.0f;
    else if (wy < heightMap) val = (wy - (heightMap - 4)) / 4.0f;

    float n = m_densityNoise.GetNoise((float)wx, (float)wy, (float)wz);
    val += n * 0.3f;

    return val;
}

float TerrainGen::sampleCave(int wx, int wy, int wz) const {
    return m_caveNoise.GetNoise((float)wx, (float)wy, (float)wz);
}

void TerrainGen::generate(Chunk& chunk) const {
    int cx = chunk.position().x;
    int cz = chunk.position().y;

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                int wx = cx * CHUNK_SIZE + x;
                int wz = cz * CHUNK_SIZE + z;

                float height = sampleHeight(wx, wz);

                if (y == 0) {
                    chunk.setBlock(x, y, z, BlockType::Bedrock);
                } else if (y < height - 4) {
                    float cave = sampleCave(wx, y, wz);
                    if (cave < -0.4f) {
                        chunk.setBlock(x, y, z, BlockType::Air);
                    } else {
                        float oreNoise = m_oreNoise.GetNoise((float)wx * 0.05f, (float)y * 0.05f, (float)wz * 0.05f);
                        if (oreNoise > 0.85f && y < 15) chunk.setBlock(x, y, z, BlockType::DiamondOre);
                        else if (oreNoise > 0.80f && y < 30) chunk.setBlock(x, y, z, BlockType::GoldOre);
                        else if (oreNoise > 0.75f && y < 60) chunk.setBlock(x, y, z, BlockType::IronOre);
                        else if (oreNoise > 0.70f && y < 100) chunk.setBlock(x, y, z, BlockType::CoalOre);
                        else chunk.setBlock(x, y, z, BlockType::Stone);
                    }
                } else if (y < height - 2) {
                    chunk.setBlock(x, y, z, BlockType::Dirt);
                } else if (y < height) {
                    chunk.setBlock(x, y, z, BlockType::Dirt);
                } else if (y == (int)height) {
                    if (height > 28.0f) {
                        chunk.setBlock(x, y, z, BlockType::Grass);
                    } else {
                        chunk.setBlock(x, y, z, BlockType::Sand);
                    }
                } else {
                    chunk.setBlock(x, y, z, BlockType::Air);
                }
            }
        }
    }

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int wx = cx * CHUNK_SIZE + x;
            int wz = cz * CHUNK_SIZE + z;
            float height = sampleHeight(wx, wz);
            int topY = (int)height;

            if (topY >= CHUNK_HEIGHT - 10) continue;

            if (chunk.getBlock(x, topY, z) == BlockType::Grass) {
                float t = m_treeNoise.GetNoise((float)wx, (float)wz);
                if (t > 0.65f) {
                    for (int ty = 1; ty <= 4; ty++) {
                        if (topY + ty < CHUNK_HEIGHT)
                            chunk.setBlock(x, topY + ty, z, BlockType::Wood);
                    }
                    for (int lx = -2; lx <= 2; lx++) {
                        for (int lz = -2; lz <= 2; lz++) {
                            for (int ly = 3; ly <= 5; ly++) {
                                int bx = x + lx;
                                int bz = z + lz;
                                int by = topY + ly;
                                if (bx >= 0 && bx < CHUNK_SIZE && bz >= 0 && bz < CHUNK_SIZE && by < CHUNK_HEIGHT) {
                                    if (std::abs(lx) == 2 && std::abs(lz) == 2 && (ly == 3 || ly == 5)) continue;
                                    if (std::abs(lx) == 2 && ly == 4 && std::abs(lz) == 2) continue;
                                    if (chunk.getBlock(bx, by, bz) == BlockType::Air)
                                        chunk.setBlock(bx, by, bz, BlockType::Leaves);
                                }
                            }
                        }
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
