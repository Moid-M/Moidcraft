#include "vulkan_context.hpp"
#include "core/types.hpp"
#include "core/vk_check.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <limits>
#include <fstream>

VulkanContext::VulkanContext() = default;
VulkanContext::~VulkanContext() = default;

void VulkanContext::init(const std::vector<const char*>& instanceExtensions, VkSurfaceKHR surface, int width, int height) {
    m_surface = surface;
    if (m_instance == VK_NULL_HANDLE) {
        createInstance(instanceExtensions);
    }
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createCommandPool();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

void VulkanContext::cleanup() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);

        cleanupSwapchain();

        if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        if (m_renderPass) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        if (m_device) vkDestroyDevice(m_device, nullptr);

        if (m_debugMessenger && m_enableValidationLayers) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) func(m_instance, m_debugMessenger, nullptr);
        }

        if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        if (m_instance) vkDestroyInstance(m_instance, nullptr);
    }
}

void VulkanContext::recreateSwapchain(int width, int height) {
    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();
    m_swapchainExtent = {static_cast<u32>(width), static_cast<u32>(height)};
    recreateSwapchainInternal();
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

void VulkanContext::createInstance(const std::vector<const char*>& extensions) {
    if (m_enableValidationLayers && !checkValidationLayerSupport()) {
        std::cerr << "Validation layers requested but not available" << std::endl;
        m_enableValidationLayers = false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Minecraft Clone";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> allExts = extensions;
    if (m_enableValidationLayers) {
        allExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<u32>(allExts.size());
    createInfo.ppEnabledExtensionNames = allExts.data();

    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<u32>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance (err: " + std::to_string(result) + ")");
    }
}

void VulkanContext::setupDebugMessenger() {
    if (!m_enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        func(m_instance, &createInfo, nullptr, &m_debugMessenger);
    }
}

void VulkanContext::pickPhysicalDevice() {
    u32 count;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPUs found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);

        QueueFamilyIndices indices = findQueueFamilies(device);
        if (indices.isComplete()) {
            m_physicalDevice = device;
            m_msaaSamples = getMaxUsableSampleCount();
            std::cout << "Selected GPU: " << props.deviceName << std::endl;
            return;
        }
    }
    throw std::runtime_error("No suitable GPU found");
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<u32> uniqueFamilies = {indices.graphics.value(), indices.present.value()};
    float queuePriority = 1.0f;

    for (u32 family : uniqueFamilies) {
        queueCreateInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;

    std::vector<const char*> exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (m_enableValidationLayers) {
        exts.push_back("VK_KHR_shader_non_semantic_info");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<u32>(exts.size());
    createInfo.ppEnabledExtensionNames = exts.data();

    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<u32>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    }

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, indices.graphics.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.present.value(), 0, &m_presentQueue);
    if (indices.transfer.has_value())
        vkGetDeviceQueue(m_device, indices.transfer.value(), 0, &m_transferQueue);
    else
        m_transferQueue = m_graphicsQueue;
}

void VulkanContext::createSwapchain() {
    SwapChainSupport support = querySwapChainSupport(m_physicalDevice);
    auto format = chooseSwapSurfaceFormat(support.formats);
    auto presentMode = chooseSwapPresentMode(support.presentModes);
    auto extent = chooseSwapExtent(support.capabilities);

    m_swapchainFormat = format.format;
    m_swapchainExtent = extent;

    u32 imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    u32 famIndices[] = {indices.graphics.value(), indices.present.value()};

    if (indices.graphics != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = famIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));
}

void VulkanContext::createImageViews() {
    u32 count;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr);
    m_swapchainImages.resize(count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_swapchainImages.data());

    m_swapchainImageViews.resize(count);
    for (size_t i = 0; i < count; i++) {
        m_swapchainImageViews[i] = createImageView(m_swapchainImages[i], m_swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanContext::createRenderPass() {
    // Color attachment (MSAA resolve target or main)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = m_msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = m_msaaSamples > VK_SAMPLE_COUNT_1_BIT ?
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = m_msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color resolve attachment (for MSAA)
    VkAttachmentDescription colorResolve{};
    colorResolve.format = m_swapchainFormat;
    colorResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT)
        subpass.pResolveAttachments = &resolveRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments = {colorAttachment, depthAttachment};
    if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT)
        attachments.push_back(colorResolve);

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<u32>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    VK_CHECK(vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass));
}

void VulkanContext::createCommandPool() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = indices.graphics.value();
    VK_CHECK(vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool));
}

void VulkanContext::createColorResources() {
    if (m_msaaSamples == VK_SAMPLE_COUNT_1_BIT) return;
    VkFormat fmt = m_swapchainFormat;
    createImage(m_swapchainExtent.width, m_swapchainExtent.height, fmt,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_colorImage, m_colorImageMemory);
    m_colorImageView = createImageView(m_colorImage, fmt, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanContext::createDepthResources() {
    VkFormat fmt = VK_FORMAT_D32_SFLOAT;
    createImage(m_swapchainExtent.width, m_swapchainExtent.height, fmt,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_depthImage, m_depthImageMemory);
    m_depthImageView = createImageView(m_depthImage, fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanContext::createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        std::vector<VkImageView> attachments;
        if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
            attachments = {m_colorImageView, m_depthImageView, m_swapchainImageViews[i]};
        } else {
            attachments = {m_swapchainImageViews[i], m_depthImageView};
        }

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;
        info.attachmentCount = static_cast<u32>(attachments.size());
        info.pAttachments = attachments.data();
        info.width = m_swapchainExtent.width;
        info.height = m_swapchainExtent.height;
        info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(m_device, &info, nullptr, &m_framebuffers[i]));
    }
}

void VulkanContext::cleanupSwapchain() {
    for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);

    if (m_depthImageView) vkDestroyImageView(m_device, m_depthImageView, nullptr);
    if (m_depthImage) vkDestroyImage(m_device, m_depthImage, nullptr);
    if (m_depthImageMemory) vkFreeMemory(m_device, m_depthImageMemory, nullptr);

    if (m_colorImageView) vkDestroyImageView(m_device, m_colorImageView, nullptr);
    if (m_colorImage) vkDestroyImage(m_device, m_colorImage, nullptr);
    if (m_colorImageMemory) vkFreeMemory(m_device, m_colorImageMemory, nullptr);

    if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    m_framebuffers.clear();
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
    m_depthImage = VK_NULL_HANDLE;
    m_depthImageMemory = VK_NULL_HANDLE;
    m_depthImageView = VK_NULL_HANDLE;
    m_colorImage = VK_NULL_HANDLE;
    m_colorImageMemory = VK_NULL_HANDLE;
    m_colorImageView = VK_NULL_HANDLE;
}

void VulkanContext::recreateSwapchainInternal() {
    SwapChainSupport support = querySwapChainSupport(m_physicalDevice);
    auto format = chooseSwapSurfaceFormat(support.formats);
    auto presentMode = chooseSwapPresentMode(support.presentModes);

    m_swapchainFormat = format.format;

    u32 imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = m_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    u32 famIndices[] = {indices.graphics.value(), indices.present.value()};
    if (indices.graphics != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = famIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));
}

// ---- Helper implementations ----

VkCommandBuffer VulkanContext::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandPool = m_commandPool;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &alloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

u32 VulkanContext::findMemoryType(u32 typeFilter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    u32 count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (u32 i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics = i;
        if (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
            !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) indices.transfer = i;

        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present);
        if (present) indices.present = i;

        if (indices.isComplete()) break;
    }
    if (!indices.transfer.has_value()) indices.transfer = indices.graphics;
    return indices;
}

SwapChainSupport VulkanContext::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &support.capabilities);

    u32 count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &count, nullptr);
    support.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &count, support.formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &count, nullptr);
    support.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &count, support.presentModes.data());
    return support;
}

bool VulkanContext::checkValidationLayerSupport() const {
    u32 count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (const char* name : m_validationLayers) {
        bool found = false;
        for (const auto& prop : layers) {
            if (strcmp(name, prop.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

VkSampleCountFlagBits VulkanContext::getMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    auto counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    for (const auto& m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) const {
    if (caps.currentExtent.width != std::numeric_limits<u32>::max())
        return caps.currentExtent;
    return {std::clamp(m_swapchainExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(m_swapchainExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
}

void VulkanContext::createImage(u32 w, u32 h, VkFormat fmt, VkImageTiling tiling,
                                VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                                VkImage& image, VkDeviceMemory& imageMemory) const {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = {w, h, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = fmt;
    info.tiling = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.samples = m_msaaSamples;
    info.flags = 0;
    VK_CHECK(vkCreateImage(m_device, &info, nullptr, &image));

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(m_device, image, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
    VK_CHECK(vkAllocateMemory(m_device, &alloc, nullptr, &imageMemory));
    vkBindImageMemory(m_device, image, imageMemory, 0);
}

VkImageView VulkanContext::createImageView(VkImage image, VkFormat fmt, VkImageAspectFlags aspect) const {
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = fmt;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    VkImageView view;
    VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &view));
    return view;
}

void VulkanContext::transitionImageLayout(VkImage image, VkFormat fmt,
                                          VkImageLayout oldLayout, VkImageLayout newLayout) const {
    auto cmd = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

void VulkanContext::copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) const {
    auto cmd = beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
}

bool VulkanContext::hasStencilComponent(VkFormat fmt) const {
    return fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData) {
    std::cerr << "[Vulkan] " << data->pMessage << std::endl;
    return VK_FALSE;
}
