#include "vulkan_mesh.hpp"
#include "core/vk_check.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

VulkanMesh::~VulkanMesh() { cleanup(); }

void VulkanMesh::init(VulkanContext* ctx) {
    m_ctx = ctx;
}

void VulkanMesh::upload(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    cleanup();

    m_vertexCount = static_cast<u32>(vertices.size());
    m_indexCount = static_cast<u32>(indices.size());

    if (m_vertexCount == 0) return;

    VkDeviceSize vertexSize = sizeof(Vertex) * m_vertexCount;
    VkDeviceSize indexSize = sizeof(u32) * m_indexCount;

    // Staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(vertexSize + indexSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(m_ctx->device(), stagingMemory, 0, vertexSize + indexSize, 0, &data);
    memcpy(data, vertices.data(), vertexSize);
    if (m_indexCount > 0) memcpy(static_cast<u8*>(data) + vertexSize, indices.data(), indexSize);
    vkUnmapMemory(m_ctx->device(), stagingMemory);

    // Vertex buffer (device local)
    createBuffer(vertexSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_vertexBuffer, m_vertexMemory);

    // Index buffer (device local)
    if (m_indexCount > 0) {
        createBuffer(indexSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_indexBuffer, m_indexMemory);
    }

    // Copy
    auto cmd = m_ctx->beginSingleTimeCommands();

    VkBufferCopy copy{};
    copy.size = vertexSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, m_vertexBuffer, 1, &copy);
    if (m_indexCount > 0) {
        copy.srcOffset = vertexSize;
        copy.dstOffset = 0;
        copy.size = indexSize;
        vkCmdCopyBuffer(cmd, stagingBuffer, m_indexBuffer, 1, &copy);
    }

    m_ctx->endSingleTimeCommands(cmd);

    vkDestroyBuffer(m_ctx->device(), stagingBuffer, nullptr);
    vkFreeMemory(m_ctx->device(), stagingMemory, nullptr);
}

void VulkanMesh::cleanup() {
    if (!m_ctx || !m_ctx->device()) return;
    auto dev = m_ctx->device();
    if (m_vertexBuffer) vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
    if (m_vertexMemory) vkFreeMemory(dev, m_vertexMemory, nullptr);
    if (m_indexBuffer) vkDestroyBuffer(dev, m_indexBuffer, nullptr);
    if (m_indexMemory) vkFreeMemory(dev, m_indexMemory, nullptr);
    m_vertexCount = 0;
    m_indexCount = 0;
}

void VulkanMesh::bind(VkCommandBuffer cmd) const {
    if (!valid()) return;
    VkBuffer buffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    if (m_indexCount > 0)
        vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanMesh::draw(VkCommandBuffer cmd) const {
    if (!valid()) return;
    if (m_indexCount > 0)
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
    else
        vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);
}

void VulkanMesh::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags props, VkBuffer& buffer,
                              VkDeviceMemory& memory) const {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(m_ctx->device(), &info, nullptr, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(m_ctx->device(), buffer, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = m_ctx->findMemoryType(req.memoryTypeBits, props);
    VK_CHECK(vkAllocateMemory(m_ctx->device(), &alloc, nullptr, &memory));
    vkBindBufferMemory(m_ctx->device(), buffer, memory, 0);
}
