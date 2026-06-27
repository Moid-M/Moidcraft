#pragma once
#include "vulkan_context.hpp"
#include <vector>
#include <string>

class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void init(VulkanContext* ctx, const std::string& vertShader, const std::string& fragShader,
              VkVertexInputBindingDescription bindingDesc,
              std::vector<VkVertexInputAttributeDescription> attribDescs,
              VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE,
              VkPushConstantRange pushConstant = {},
              bool depthTest = true, bool transparent = false,
              VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
              VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    void cleanup();

    VkPipeline pipeline() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }

private:
    VulkanContext* m_ctx = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;

    VkShaderModule createShader(const std::string& path);
};


