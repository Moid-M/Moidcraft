#pragma once
#include "vulkan_context.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_mesh.hpp"
#include "core/camera.hpp"
#include "core/texture_manager.hpp"
#include "core/types.hpp"
#include <memory>
#include <vector>

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 fogColor;
    alignas(16) glm::vec4 cameraPos;
    alignas(4) float time;
    alignas(4) float gamma;
    alignas(4) float alphaDiscard;
    alignas(4) float _pad0;
};

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void init(VulkanContext* ctx, const Camera* camera);
    void cleanup();

    void setTextureManager(TextureManager* texMgr) { m_texMgr = texMgr; }
    void setUnderwater(bool v) { m_underwater = v; }
    void setGamma(float g) { m_gamma = std::clamp(g, 0.5f, 1.5f); }
    void beginFrame();
    void endFrame();

    void drawMesh(const VulkanMesh& mesh, const glm::mat4& model);
    void drawMeshTransparent(const VulkanMesh& mesh, const glm::mat4& model);
    void drawMeshDebug(const VulkanMesh& mesh, const glm::mat4& model);

    VkCommandBuffer currentCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }
    int currentFrame() const { return m_currentFrame; }
    void updateUniforms();

    void waitIdle();

private:
    VulkanContext* m_ctx = nullptr;
    const Camera* m_camera = nullptr;
    TextureManager* m_texMgr = nullptr;
    bool m_underwater = false;
    float m_gamma = 1.0f;


    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSets[MAX_FRAMES_IN_FLIGHT];

    VkBuffer m_uniformBuffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory m_uniformMemories[MAX_FRAMES_IN_FLIGHT];
    void* m_uniformMapped[MAX_FRAMES_IN_FLIGHT]{};

    VkCommandBuffer m_commandBuffers[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore m_imageAvailable[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore m_renderFinished[MAX_FRAMES_IN_FLIGHT];
    VkFence m_inFlightFences[MAX_FRAMES_IN_FLIGHT];

    int m_currentFrame = 0;
    u32 m_imageIndex = 0;

    std::unique_ptr<VulkanPipeline> m_opaquePipeline;
    std::unique_ptr<VulkanPipeline> m_transparentPipeline;
    std::unique_ptr<VulkanPipeline> m_debugPipeline;

    void createDescriptorPool();
    void createDescriptorLayout();
    void createDescriptorSets();
    void createUniformBuffers();
    void createCommandBuffers();
    void createSyncObjects();
    void recordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex);

    void createPipeline();
};
