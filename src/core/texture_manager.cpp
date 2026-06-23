#include "texture_manager.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <zlib.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

TextureManager::TextureManager() = default;
TextureManager::~TextureManager() { cleanupVulkan(); }

#pragma pack(push, 1)
struct ZipLocalFileHeader {
    u32 signature = 0x04034b50;
    u16 versionNeeded;
    u16 flags;
    u16 compressionMethod;
    u16 lastModTime;
    u16 lastModDate;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
    u16 filenameLength;
    u16 extraFieldLength;
};

struct ZipCentralDirEntry {
    u32 signature = 0x02014b50;
    u16 versionMadeBy;
    u16 versionNeeded;
    u16 flags;
    u16 compressionMethod;
    u16 lastModTime;
    u16 lastModDate;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
    u16 filenameLength;
    u16 extraFieldLength;
    u16 commentLength;
    u16 diskNumberStart;
    u16 internalAttribs;
    u32 externalAttribs;
    u32 localHeaderOffset;
};

struct ZipEndCentralDir {
    u32 signature = 0x06054b50;
    u16 diskNumber;
    u16 diskStart;
    u16 numEntries;
    u16 totalEntries;
    u32 centralDirSize;
    u32 centralDirOffset;
    u16 commentLength;
};
#pragma pack(pop)

bool TextureManager::loadResourcePack(const std::string& path) {
    m_textures.clear();
    m_nameMap.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open resource pack: " << path << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> zipData(fileSize);
    file.read(reinterpret_cast<char*>(zipData.data()), fileSize);
    file.close();

    // Find End of Central Directory
    ZipEndCentralDir eocd;
    bool foundEocd = false;
    for (int i = (int)fileSize - 22; i >= 0; i--) {
        if (*(u32*)&zipData[i] == 0x06054b50) {
            memcpy(&eocd, &zipData[i], sizeof(eocd));
            foundEocd = true;
            break;
        }
    }
    if (!foundEocd) {
        std::cerr << "Invalid ZIP file (no EOCD)" << std::endl;
        return false;
    }

    // Read central directory entries
    int numEntries = eocd.numEntries;
    u32 centralDirOffset = eocd.centralDirOffset;

    const std::string prefix = "assets/minecraft/textures/block/";

    for (int i = 0; i < numEntries; i++) {
        ZipCentralDirEntry entry;
        memcpy(&entry, &zipData[centralDirOffset], sizeof(entry));

        std::string name((const char*)&zipData[centralDirOffset + sizeof(entry)], entry.filenameLength);
        centralDirOffset += sizeof(entry) + entry.filenameLength + entry.extraFieldLength + entry.commentLength;

        // Skip directories
        if (name.back() == '/') continue;
        // Only process PNG files in block textures directory
        if (name.find(prefix) != 0) continue;

        std::string ext = ".png";
        if (name.size() < ext.size() ||
            name.compare(name.size() - ext.size(), ext.size(), ext) != 0) continue;

        std::string blockName = name.substr(prefix.size());
        blockName = blockName.substr(0, blockName.size() - ext.size());
        if (blockName.find('/') != std::string::npos) continue;

        // Read local file header to get the data
        u32 localOffset = entry.localHeaderOffset;
        ZipLocalFileHeader local;
        memcpy(&local, &zipData[localOffset], sizeof(local));
        u32 dataOffset = localOffset + sizeof(local) + local.filenameLength + local.extraFieldLength;

        std::vector<u8> pixels;
        if (entry.compressionMethod == 0) {
            // Stored
            pixels.resize(entry.uncompressedSize);
            memcpy(pixels.data(), &zipData[dataOffset], entry.uncompressedSize);
        } else if (entry.compressionMethod == 8) {
            // Deflated
            pixels.resize(entry.uncompressedSize);
            z_stream strm{};
            strm.next_in = (Bytef*)&zipData[dataOffset];
            strm.avail_in = entry.compressedSize;
            strm.next_out = (Bytef*)pixels.data();
            strm.avail_out = entry.uncompressedSize;
            inflateInit2(&strm, -MAX_WBITS);
            inflate(&strm, Z_FINISH);
            inflateEnd(&strm);
        } else {
            continue;
        }

        if (loadTexture(blockName, pixels.data(), pixels.size())) {
            std::cout << "  Loaded: " << blockName << std::endl;
        }
    }

    if (m_textures.empty()) {
        std::cerr << "No block textures found in resource pack" << std::endl;
        return false;
    }

    std::cout << "Loaded " << m_textures.size() << " block textures" << std::endl;
    buildAtlas();
    return true;
}

bool TextureManager::loadResourcePackFromMemory(const std::vector<u8>& data) {
    (void)data;
    return false;
}

static void fillChecker(u8* pixels, int size, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2, int checkSize = 8) {
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            bool checker = ((x / checkSize) + (y / checkSize)) % 2 == 0;
            if (checker) {
                pixels[idx+0] = r1; pixels[idx+1] = g1; pixels[idx+2] = b1;
            } else {
                pixels[idx+0] = r2; pixels[idx+1] = g2; pixels[idx+2] = b2;
            }
            pixels[idx+3] = 255;
        }
    }
}

static void fillSolid(u8* pixels, int size, u8 r, u8 g, u8 b, u8 a = 255) {
    for (int i = 0; i < size * size; i++) {
        int idx = i * 4;
        pixels[idx+0] = r; pixels[idx+1] = g; pixels[idx+2] = b; pixels[idx+3] = a;
    }
}

static void fillStriped(u8* pixels, int size, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2, int stripeH = 2) {
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            bool stripe = (y / stripeH) % 2 == 0;
            if (stripe) {
                pixels[idx+0] = r1; pixels[idx+1] = g1; pixels[idx+2] = b1;
            } else {
                pixels[idx+0] = r2; pixels[idx+1] = g2; pixels[idx+2] = b2;
            }
            pixels[idx+3] = 255;
        }
    }
}

static void fillNoisy(u8* pixels, int size, u8 r, u8 g, u8 b, int seed = 0) {
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            int n = ((x * 7 + y * 31 + seed * 13) & 0xff);
            int variation = (n % 40) - 20;
            pixels[idx+0] = (u8)std::clamp((int)r + variation, 0, 255);
            pixels[idx+1] = (u8)std::clamp((int)g + variation, 0, 255);
            pixels[idx+2] = (u8)std::clamp((int)b + variation, 0, 255);
            pixels[idx+3] = 255;
        }
    }
}

void TextureManager::generateProceduralTextures() {
    m_textures.clear();
    m_nameMap.clear();

    auto addTex = [&](const std::string& name, const std::vector<u8>& pixels) {
        TextureEntry entry;
        entry.name = name;
        entry.width = TILE_SIZE;
        entry.height = TILE_SIZE;
        entry.channels = 4;
        entry.pixels = pixels;
        int idx = (int)m_textures.size();
        m_textures.push_back(std::move(entry));
        m_nameMap[name] = idx;
    };

    std::vector<u8> buf(TILE_SIZE * TILE_SIZE * 4);

    // Grass block top (green with noise)
    fillNoisy(buf.data(), TILE_SIZE, 80, 160, 50, 1);
    addTex("grass_block_top", buf);

    // Grass block side (brown with green strip)
    fillNoisy(buf.data(), TILE_SIZE, 120, 90, 50, 2);
    for (int x = 0; x < TILE_SIZE; x++) {
        int idx = (0 * TILE_SIZE + x) * 4;
        buf[idx+0] = 60; buf[idx+1] = 140; buf[idx+2] = 40; buf[idx+3] = 255;
    }
    for (int x = 0; x < TILE_SIZE; x++) {
        int idx = (1 * TILE_SIZE + x) * 4;
        buf[idx+0] = 50; buf[idx+1] = 120; buf[idx+2] = 35; buf[idx+3] = 255;
    }
    addTex("grass_block_side", buf);

    // Dirt (brown noisy)
    fillNoisy(buf.data(), TILE_SIZE, 130, 100, 60, 3);
    addTex("dirt", buf);

    // Stone (gray noisy)
    fillNoisy(buf.data(), TILE_SIZE, 110, 110, 110, 4);
    addTex("stone", buf);

    // Cobblestone (gray checker)
    fillChecker(buf.data(), TILE_SIZE, 110, 110, 110, 90, 90, 90, 4);
    addTex("cobblestone", buf);

    // Oak log top (brown rings)
    fillSolid(buf.data(), TILE_SIZE, 140, 110, 60);
    for (int r = 3; r < 8; r += 2) {
        for (int y = 0; y < TILE_SIZE; y++) {
            for (int x = 0; x < TILE_SIZE; x++) {
                int dx = x - 8, dy = y - 8;
                int dist = (int)std::sqrt(dx * dx + dy * dy);
                if (dist == r) {
                    int idx = (y * TILE_SIZE + x) * 4;
                    buf[idx+0] = 110; buf[idx+1] = 80; buf[idx+2] = 40;
                }
            }
        }
    }
    addTex("oak_log_top", buf);

    // Oak log side (brown with stripes)
    fillStriped(buf.data(), TILE_SIZE, 150, 120, 70, 120, 90, 50, 3);
    addTex("oak_log", buf);

    // Oak leaves (dark green with transparency)
    fillSolid(buf.data(), TILE_SIZE, 0, 0, 0, 0);
    for (int y = 0; y < TILE_SIZE; y++) {
        for (int x = 0; x < TILE_SIZE; x++) {
            int n = ((x * 13 + y * 37) & 0xff);
            if (n > 80 && n < 240) {
                int idx = (y * TILE_SIZE + x) * 4;
                int shade = n % 60;
                buf[idx+0] = (u8)(40 + shade);
                buf[idx+1] = (u8)(90 + shade);
                buf[idx+2] = (u8)(30 + shade / 2);
                buf[idx+3] = 255;
            }
        }
    }
    addTex("oak_leaves", buf);

    // Oak planks
    fillStriped(buf.data(), TILE_SIZE, 170, 140, 90, 150, 120, 75, 4);
    addTex("oak_planks", buf);

    // Sand
    fillNoisy(buf.data(), TILE_SIZE, 210, 200, 140, 5);
    addTex("sand", buf);

    // Water (blue semi-transparent)
    fillNoisy(buf.data(), TILE_SIZE, 40, 80, 180, 6);
    for (int i = 0; i < TILE_SIZE * TILE_SIZE; i++) {
        buf[i * 4 + 3] = 160;
    }
    addTex("water_still", buf);

    // Glass (near-transparent light blue)
    fillSolid(buf.data(), TILE_SIZE, 180, 220, 255, 80);
    addTex("glass", buf);

    // Bricks
    fillSolid(buf.data(), TILE_SIZE, 160, 80, 60);
    for (int y = 0; y < TILE_SIZE; y += 4) {
        int offset = (y / 4) % 2 == 0 ? 0 : 4;
        for (int x = 0; x < TILE_SIZE; x += 8) {
            for (int px = 0; px < 8; px++) {
                int bx = x + px + offset;
                if (bx >= TILE_SIZE) continue;
                int idx = ((y + 3) * TILE_SIZE + bx) * 4;
                buf[idx+0] = 130; buf[idx+1] = 65; buf[idx+2] = 50;
            }
        }
    }
    for (int i = 0; i < TILE_SIZE * TILE_SIZE * 4; i += 4) {
        int x = (i / 4) % TILE_SIZE, y = (i / 4) / TILE_SIZE;
        if (x % 8 == 0 || y % 4 == 0) {
            buf[i+0] = (u8)std::min((int)buf[i+0] + 30, 255);
            buf[i+1] = (u8)std::min((int)buf[i+1] + 15, 255);
            buf[i+2] = (u8)std::min((int)buf[i+2] + 10, 255);
        }
    }
    addTex("bricks", buf);

    // Bedrock (dark gray)
    fillNoisy(buf.data(), TILE_SIZE, 70, 60, 55, 7);
    addTex("bedrock", buf);

    // Snow (white)
    fillNoisy(buf.data(), TILE_SIZE, 230, 235, 245, 8);
    addTex("snow", buf);

    // Gravel
    fillNoisy(buf.data(), TILE_SIZE, 140, 130, 120, 9);
    addTex("gravel", buf);

    // Coal ore
    fillNoisy(buf.data(), TILE_SIZE, 110, 110, 110, 10);
    for (int n = 0; n < 5; n++) {
        int sx = (n * 7 + 3) % TILE_SIZE, sy = (n * 13 + 5) % TILE_SIZE;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < TILE_SIZE && py >= 0 && py < TILE_SIZE) {
                    int idx = (py * TILE_SIZE + px) * 4;
                    buf[idx+0] = 30; buf[idx+1] = 30; buf[idx+2] = 35;
                }
            }
        }
    }
    addTex("coal_ore", buf);

    // Iron ore
    fillNoisy(buf.data(), TILE_SIZE, 110, 110, 110, 11);
    for (int n = 0; n < 4; n++) {
        int sx = (n * 11 + 7) % TILE_SIZE, sy = (n * 17 + 3) % TILE_SIZE;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < TILE_SIZE && py >= 0 && py < TILE_SIZE) {
                    int idx = (py * TILE_SIZE + px) * 4;
                    buf[idx+0] = 200; buf[idx+1] = 180; buf[idx+2] = 150;
                }
            }
        }
    }
    addTex("iron_ore", buf);

    // Gold ore
    fillNoisy(buf.data(), TILE_SIZE, 110, 110, 110, 12);
    for (int n = 0; n < 4; n++) {
        int sx = (n * 11 + 7) % TILE_SIZE, sy = (n * 17 + 3) % TILE_SIZE;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < TILE_SIZE && py >= 0 && py < TILE_SIZE) {
                    int idx = (py * TILE_SIZE + px) * 4;
                    buf[idx+0] = 230; buf[idx+1] = 210; buf[idx+2] = 80;
                }
            }
        }
    }
    addTex("gold_ore", buf);

    // Diamond ore
    fillNoisy(buf.data(), TILE_SIZE, 110, 110, 110, 13);
    for (int n = 0; n < 3; n++) {
        int sx = (n * 11 + 7) % TILE_SIZE, sy = (n * 17 + 3) % TILE_SIZE;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < TILE_SIZE && py >= 0 && py < TILE_SIZE) {
                    int idx = (py * TILE_SIZE + px) * 4;
                    buf[idx+0] = 80; buf[idx+1] = 220; buf[idx+2] = 200;
                }
            }
        }
    }
    addTex("diamond_ore", buf);

    std::cout << "Generated " << m_textures.size() << " procedural textures" << std::endl;
    buildAtlas();
}

bool TextureManager::loadTexture(const std::string& name, const u8* data, size_t size) {
    int w, h, channels;
    unsigned char* pixels = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
    if (!pixels) {
        std::cerr << "  Failed to load texture: " << name << std::endl;
        return false;
    }

    if (w != TILE_SIZE || h != TILE_SIZE) {
        stbi_image_free(pixels);
        return false;
    }

    TextureEntry entry;
    entry.name = name;
    entry.width = w;
    entry.height = h;
    entry.channels = 4;
    entry.pixels.resize(w * h * 4);
    memcpy(entry.pixels.data(), pixels, w * h * 4);
    stbi_image_free(pixels);

    int idx = (int)m_textures.size();
    m_textures.push_back(std::move(entry));
    m_nameMap[name] = idx;
    return true;
}

void TextureManager::buildAtlas() {
    m_regions.clear();

    int texCount = (int)m_textures.size();
    int cols = (int)std::ceil(std::sqrt((float)texCount));
    int rows = (int)std::ceil((float)texCount / cols);

    m_atlasSize = std::max(cols, rows) * TILE_SIZE;
    m_atlasSize = std::max(m_atlasSize, 64);
    int p2 = 1;
    while (p2 < m_atlasSize) p2 <<= 1;
    m_atlasSize = p2;

    std::vector<u8> atlasPixels(m_atlasSize * m_atlasSize * 4, 0);

    for (int i = 0; i < texCount; i++) {
        int col = i % cols;
        int row = i / cols;
        int destX = col * TILE_SIZE;
        int destY = row * TILE_SIZE;

        AtlasRegion reg;
        reg.x = destX;
        reg.y = destY;
        reg.w = TILE_SIZE;
        reg.h = TILE_SIZE;
        reg.u1 = (float)destX / m_atlasSize;
        reg.v1 = (float)destY / m_atlasSize;
        reg.u2 = (float)(destX + TILE_SIZE) / m_atlasSize;
        reg.v2 = (float)(destY + TILE_SIZE) / m_atlasSize;
        m_regions.push_back(reg);

        auto& tex = m_textures[i];
        for (int y = 0; y < TILE_SIZE; y++) {
            memcpy(&atlasPixels[((destY + y) * m_atlasSize + destX) * 4],
                   &tex.pixels[y * TILE_SIZE * 4], TILE_SIZE * 4);
        }
    }

    if (!m_textures.empty()) {
        m_textures[0].pixels = std::move(atlasPixels);
        m_textures[0].width = m_atlasSize;
        m_textures[0].height = m_atlasSize;
    }
}

void TextureManager::createVulkanTexture(VkDevice device, VkPhysicalDevice physDev,
                                          VkCommandPool cmdPool, VkQueue graphicsQueue) {
    m_device = device;
    if (m_textures.empty()) return;

    auto& atlas = m_textures[0];
    VkDeviceSize imageSize = atlas.width * atlas.height * 4;

    u32 mipLevels = 1;
    {
        u32 dim = std::max(atlas.width, atlas.height);
        while (dim > 1) { dim >>= 1; mipLevels++; }
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &req);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);

    u32 memType = ~0u;
    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((req.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memType = i;
            break;
        }
    }

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = memType;
    vkAllocateMemory(device, &alloc, nullptr, &stagingMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, atlas.pixels.data(), imageSize);
    vkUnmapMemory(device, stagingMemory);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = {(u32)atlas.width, (u32)atlas.height, 1};
    imgInfo.mipLevels = mipLevels;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vkCreateImage(device, &imgInfo, nullptr, &m_image);

    vkGetImageMemoryRequirements(device, m_image, &req);
    alloc.allocationSize = req.size;
    memType = ~0u;
    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((req.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memType = i;
            break;
        }
    }
    alloc.memoryTypeIndex = memType;
    vkAllocateMemory(device, &alloc, nullptr, &m_imageMemory);
    vkBindImageMemory(device, m_image, m_imageMemory, 0);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {(u32)atlas.width, (u32)atlas.height, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    for (u32 i = 1; i < mipLevels; i++) {
        VkImageMemoryBarrier preBarrier{};
        preBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        preBarrier.image = m_image;
        preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        preBarrier.subresourceRange.baseMipLevel = i - 1;
        preBarrier.subresourceRange.levelCount = 1;
        preBarrier.subresourceRange.layerCount = 1;
        preBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        preBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        preBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &preBarrier);

        int32_t srcW = atlas.width >> (i - 1);
        int32_t srcH = atlas.height >> (i - 1);
        int32_t dstW = atlas.width >> i;
        int32_t dstH = atlas.height >> i;
        if (dstW < 1) dstW = 1;
        if (dstH < 1) dstH = 1;

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {srcW, srcH, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {dstW, dstH, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        VkImageMemoryBarrier postBarrier{};
        postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postBarrier.image = m_image;
        postBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        postBarrier.subresourceRange.baseMipLevel = i - 1;
        postBarrier.subresourceRange.levelCount = 1;
        postBarrier.subresourceRange.layerCount = 1;
        postBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &postBarrier);
    }

    {
        VkImageMemoryBarrier lastBarrier{};
        lastBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        lastBarrier.image = m_image;
        lastBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        lastBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        lastBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        lastBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
        lastBarrier.subresourceRange.levelCount = 1;
        lastBarrier.subresourceRange.layerCount = 1;
        lastBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        lastBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        lastBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        lastBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &lastBarrier);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &m_imageView);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)mipLevels;
    vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler);
}

void TextureManager::cleanupVulkan() {
    if (m_device) {
        if (m_imageView) vkDestroyImageView(m_device, m_imageView, nullptr);
        if (m_sampler) vkDestroySampler(m_device, m_sampler, nullptr);
        if (m_image) vkDestroyImage(m_device, m_image, nullptr);
        if (m_imageMemory) vkFreeMemory(m_device, m_imageMemory, nullptr);
    }
    m_image = VK_NULL_HANDLE;
    m_imageView = VK_NULL_HANDLE;
    m_sampler = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

int TextureManager::getTextureIndex(const std::string& name) const {
    auto it = m_nameMap.find(name);
    return it != m_nameMap.end() ? it->second : -1;
}

const AtlasRegion& TextureManager::getRegion(int index) const {
    static AtlasRegion empty{};
    if (index < 0 || index >= (int)m_regions.size()) return empty;
    return m_regions[index];
}

VkDescriptorImageInfo TextureManager::descriptorInfo() const {
    return {
        .sampler = m_sampler,
        .imageView = m_imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}
