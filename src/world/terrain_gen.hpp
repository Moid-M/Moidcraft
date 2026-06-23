#pragma once
#include "core/types.hpp"
#include "block.hpp"
#include "chunk.hpp"
#include "FastNoiseLite.h"

class TerrainGen {
public:
    TerrainGen(int seed = 42);

    void generate(Chunk& chunk) const;
    void generateFlat(Chunk& chunk) const;

private:
    int m_seed;
    mutable FastNoiseLite m_heightNoise;
    mutable FastNoiseLite m_densityNoise;
    mutable FastNoiseLite m_caveNoise;
    mutable FastNoiseLite m_oreNoise;
    mutable FastNoiseLite m_treeNoise;

    float sampleHeight(int wx, int wz) const;
    float sampleDensity(int wx, int wy, int wz) const;
    float sampleCave(int wx, int wy, int wz) const;
};
