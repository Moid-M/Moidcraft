#pragma once
#include "core/types.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_pipeline.hpp"
#include "vulkan/vulkan_mesh.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>

struct UiVertex {
    float x, y;
    float u, v;
    uint8_t r, g, b, a;
};

class UiRenderer {
public:
    UiRenderer() = default;
    ~UiRenderer();

    void init(VulkanContext* ctx, const std::string& texturesDir);
    void cleanup();

    void beginFrame(int fbWidth, int fbHeight);
    void drawRect(float x, float y, float w, float h, glm::vec4 color);
    void drawText(const char* text, float x, float y, float size, glm::vec4 color);
    void drawTextFg(const char* text, float x, float y, float size, glm::vec4 color);
    void drawButton(float x, float y, float w, float h, bool hovered);
    void flushAll(VkCommandBuffer cmd);

    int textWidth(const char* text, float size) const;

private:
    VulkanContext* m_ctx = nullptr;
    std::unique_ptr<VulkanPipeline> m_textPipeline;
    std::unique_ptr<VulkanPipeline> m_buttonPipeline;
    VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_textDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet m_buttonDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    // 3 batches for correct z-ordering
    std::vector<UiVertex> m_bgVerts;   // background + title (drawn first)
    std::vector<UiVertex> m_btnVerts;   // buttons (drawn second)
    std::vector<UiVertex> m_fgVerts;    // labels + copyright (drawn last)

    VkBuffer m_bgBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_bgMem = VK_NULL_HANDLE;
    size_t m_bgCap = 0;

    VkBuffer m_btnBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_btnMem = VK_NULL_HANDLE;
    size_t m_btnCap = 0;

    VkBuffer m_fgBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_fgMem = VK_NULL_HANDLE;
    size_t m_fgCap = 0;

    VkImage m_fontImage = VK_NULL_HANDLE;
    VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
    VkImageView m_fontView = VK_NULL_HANDLE;
    VkSampler m_fontSampler = VK_NULL_HANDLE;

    VkImage m_buttonImage = VK_NULL_HANDLE;
    VkDeviceMemory m_buttonMemory = VK_NULL_HANDLE;
    VkImageView m_buttonView = VK_NULL_HANDLE;

    int m_fbWidth = 0, m_fbHeight = 0;
    bool m_hasButtons = false;

    static constexpr int BUTTON_W = 200;
    static constexpr int BUTTON_H = 20;
    static constexpr int BUTTON_TEX_H = 40;
    static constexpr int BORDER = 3;
    static constexpr int FONT_CELL = 8;
    static constexpr int FONT_COLS = 16;

    void loadFontTexture(const std::string& path);
    void loadButtonTextures(const std::string& normalPath, const std::string& highlightedPath);
    void createTextPipeline();
    void createButtonPipeline();
    void createDescriptorSets();
    void allocBatch(VkBuffer& buf, VkDeviceMemory& mem, size_t& cap, size_t maxVerts);
    void addQuad(float x, float y, float w, float h,
                 float u1, float v1, float u2, float v2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                 std::vector<UiVertex>& verts);
    void drawText(const char* text, float x, float y, float size, glm::vec4 color,
                  std::vector<UiVertex>& verts);
    void uploadAndDraw(VkCommandBuffer cmd, std::vector<UiVertex>& verts,
                       VkBuffer& buf, VkDeviceMemory& mem, size_t cap,
                       VkPipeline pipeline, VkPipelineLayout layout,
                       VkDescriptorSet descSet);
};
