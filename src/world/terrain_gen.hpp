#pragma once
#include "core/types.hpp"
#include "block.hpp"
#include "chunk.hpp"
#include "FastNoiseLite.h"

enum class BiomeType : u8 {
    Ocean = 0,
    Plains,
    Forest,
    Desert,
    Taiga,
    Mountains
};

class TerrainGen {
public:
    TerrainGen(int seed = 42);

    void generate(Chunk& chunk, Chunk* neighbors[4] = nullptr) const;
    void generateFlat(Chunk& chunk) const;

private:
    int m_seed;
    mutable FastNoiseLite m_continentalNoise;
    mutable FastNoiseLite m_elevationNoise;
    mutable FastNoiseLite m_detailNoise;
    mutable FastNoiseLite m_temperatureNoise;
    mutable FastNoiseLite m_humidityNoise;
    mutable FastNoiseLite m_caveNoise;
    mutable FastNoiseLite m_oreNoise;
    mutable FastNoiseLite m_treeNoise;

    BiomeType getBiome(float wx, float wz, float continental) const;
    float getBaseHeight(BiomeType biome, float wx, float wz) const;
};
