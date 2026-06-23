#pragma once
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>

#define VK_CHECK(x) do { VkResult err = x; if (err != VK_SUCCESS) { \
    std::cerr << "Vulkan error: " << err << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    throw std::runtime_error("Vulkan error"); \
} } while(0)
