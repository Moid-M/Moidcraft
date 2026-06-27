#pragma once
#include "core/types.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct AtlasRegion {
    u32 x, y, w, h;
    float u1, v1, u2, v2; // normalized UV coords
};

class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    bool loadResourcePack(const std::string& path);
    bool loadResourcePackFromMemory(const std::vector<u8>& data);

    void createVulkanTexture(VkDevice device, VkPhysicalDevice physDev,
                             VkCommandPool cmdPool, VkQueue graphicsQueue);

    void cleanupVulkan();

    int getTextureIndex(const std::string& name) const;
    const AtlasRegion& getRegion(int index) const;
    int textureCount() const { return m_textures.size(); }
    int atlasSize() const { return m_atlasSize; }

    VkImageView imageView() const { return m_imageView; }
    VkSampler sampler() const { return m_sampler; }
    VkDescriptorImageInfo descriptorInfo() const;

    void generateProceduralTextures();
    void fixLeafTextures();

    static constexpr int ATLAS_SIZE = 1024;
    static constexpr int TILE_SIZE = 16;

private:
    struct TextureEntry {
        std::string name;
        std::vector<u8> pixels;
        int width, height, channels;
    };

    std::vector<TextureEntry> m_textures;
    std::unordered_map<std::string, int> m_nameMap;
    std::vector<AtlasRegion> m_regions;
    int m_atlasSize = 0;

    // Vulkan
    VkDevice m_device = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    bool loadTexture(const std::string& name, const u8* data, size_t size);

    struct PackEntry {
        std::string name;
        int index;
    };
    void buildAtlas();
};
