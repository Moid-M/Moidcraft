#pragma once
#include <string>
#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "core/types.hpp"

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();
    void waitIdle();

    GLFWwindow* handle() const { return m_window; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int fbWidth() const { return m_fbWidth; }
    int fbHeight() const { return m_fbHeight; }
    bool resized() const { bool r = m_resized; m_resized = false; return r; }

    VkSurfaceKHR createSurface(VkInstance instance) const;
    std::vector<const char*> getRequiredInstanceExtensions() const;

private:
    GLFWwindow* m_window = nullptr;
    int m_width, m_height;
    int m_fbWidth, m_fbHeight;
    mutable bool m_resized = false;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};
