#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    u8  lighting; // ambient occlusion / light

    static VkVertexInputBindingDescription bindingDesc() {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    static std::array<VkVertexInputAttributeDescription, 4> attribDesc() {
        return {{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos)},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
            {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Vertex, uv)},
            {.location = 3, .binding = 0, .format = VK_FORMAT_R8_UNORM,         .offset = offsetof(Vertex, lighting)},
        }};
    }
};

struct BlockFace {
    glm::vec3 vertices[4];
    glm::vec3 normal;
};

namespace BlockFaces {
    constexpr BlockFace Front = {{
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    }, {0,0,1}};
    constexpr BlockFace Back = {{
        {1,0,0},{0,0,0},{0,1,0},{1,1,0}
    }, {0,0,-1}};
    constexpr BlockFace Right = {{
        {1,0,1},{1,0,0},{1,1,0},{1,1,1}
    }, {1,0,0}};
    constexpr BlockFace Left = {{
        {0,0,0},{0,0,1},{0,1,1},{0,1,0}
    }, {-1,0,0}};
    constexpr BlockFace Top = {{
        {0,1,1},{1,1,1},{1,1,0},{0,1,0}
    }, {0,1,0}};
    constexpr BlockFace Bottom = {{
        {0,0,0},{1,0,0},{1,0,1},{0,0,1}
    }, {0,-1,0}};

    constexpr BlockFace faces[6] = {Front, Back, Right, Left, Top, Bottom};
    constexpr glm::ivec3 dirs[6] = {
        {0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}
    };
}

constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_SIZE_SQ = CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT;

constexpr int RENDER_DISTANCE = 8;
