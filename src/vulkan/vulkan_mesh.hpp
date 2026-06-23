#pragma once
#include "vulkan_context.hpp"
#include "core/types.hpp"
#include <vector>

class VulkanMesh {
public:
    VulkanMesh() = default;
    ~VulkanMesh();

    void init(VulkanContext* ctx);
    void upload(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);
    void cleanup();

    void bind(VkCommandBuffer cmd) const;
    void draw(VkCommandBuffer cmd) const;
    bool valid() const { return m_vertexCount > 0; }

    u32 vertexCount() const { return m_vertexCount; }
    u32 indexCount() const { return m_indexCount; }

private:
    VulkanContext* m_ctx = nullptr;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexMemory = VK_NULL_HANDLE;
    u32 m_vertexCount = 0;
    u32 m_indexCount = 0;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VkBuffer& buffer,
                      VkDeviceMemory& memory) const;
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
};
