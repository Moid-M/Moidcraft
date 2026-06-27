#pragma once
#include "core/types.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>

struct QueueFamilyIndices {
    std::optional<u32> graphics;
    std::optional<u32> present;
    std::optional<u32> transfer;

    bool isComplete() const {
        return graphics.has_value() && present.has_value();
    }
};

struct SwapChainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    void init(const std::vector<const char*>& instanceExtensions, VkSurfaceKHR surface, int width, int height);
    void createInstance(const std::vector<const char*>& extensions);
    void cleanup();
    void recreateSwapchain(int width, int height);
    void recreateSwapchain();
    void setVSync(bool enabled);

    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    VkQueue transferQueue() const { return m_transferQueue; }
    const VkSwapchainKHR& swapchain() const { return m_swapchain; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkCommandPool commandPool() const { return m_commandPool; }
    VkSurfaceKHR surface() const { return m_surface; }

    const std::vector<VkImage>& swapchainImages() const { return m_swapchainImages; }
    const std::vector<VkImageView>& swapchainImageViews() const { return m_swapchainImageViews; }
    const std::vector<VkFramebuffer>& framebuffers() const { return m_framebuffers; }
    VkFormat swapchainFormat() const { return m_swapchainFormat; }
    VkExtent2D swapchainExtent() const { return m_swapchainExtent; }
    VkSampleCountFlagBits msaaSamples() const { return m_msaaSamples; }

    // Helpers
    u32 findMemoryType(u32 typeFilter, VkMemoryPropertyFlags props) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    SwapChainSupport querySwapChainSupport(VkPhysicalDevice device) const;

    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer cmd) const;

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkQueue m_transferQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent{};
    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth resources
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    // MSAA resources
    VkImage m_colorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_colorImageMemory = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;

    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createCommandPool();
    void createDepthResources();
    void createColorResources();
    void createFramebuffers();

    bool checkValidationLayerSupport() const;
    VkSampleCountFlagBits getMaxUsableSampleCount() const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) const;

    void createImage(u32 w, u32 h, VkFormat fmt, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                     VkImage& image, VkDeviceMemory& imageMemory) const;
    VkImageView createImageView(VkImage image, VkFormat fmt, VkImageAspectFlags aspect) const;
    void transitionImageLayout(VkImage image, VkFormat fmt,
                               VkImageLayout oldLayout, VkImageLayout newLayout) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) const;
    bool hasStencilComponent(VkFormat fmt) const;

    void cleanupSwapchain();
    void recreateSwapchainInternal();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* userData);

    bool m_vsyncEnabled = true;

    bool m_enableValidationLayers = true;
    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
};
