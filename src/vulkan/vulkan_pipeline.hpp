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
              bool depthTest = true, bool transparent = false);
    void cleanup();

    VkPipeline pipeline() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }

private:
    VulkanContext* m_ctx = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;

    VkShaderModule createShader(const std::string& path);
};

class VulkanDescriptorLayout {
public:
    void addBinding(u32 binding, VkDescriptorType type, VkShaderStageFlags stage, u32 count = 1);
    void build(VulkanContext* ctx);
    void cleanup();
    VkDescriptorSetLayout layout() const { return m_layout; }

private:
    VulkanContext* m_ctx = nullptr;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};
