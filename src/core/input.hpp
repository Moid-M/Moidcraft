#pragma once
#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>

class Input {
public:
    void init(GLFWwindow* window);
    void update();

    bool keyPressed(int key) const;
    bool keyDown(int key) const;
    bool keyReleased(int key) const;

    bool mouseButtonPressed(int button) const;
    bool mouseButtonDown(int button) const;
    glm::vec2 mouseDelta() const { return m_mouseDelta; }
    glm::vec2 mousePos() const { return m_mousePos; }
    double scrollDelta() const { return m_scrollDelta; }
    void setCursorMode(int mode);

private:
    GLFWwindow* m_window = nullptr;
    std::unordered_map<int, bool> m_keys;
    std::unordered_map<int, bool> m_keysPrev;
    std::unordered_map<int, bool> m_mouseBtns;
    std::unordered_map<int, bool> m_mouseBtnsPrev;
    glm::vec2 m_mousePos{0};
    glm::vec2 m_mouseDelta{0};
    double m_scrollDelta = 0.0;
    bool m_firstMouse = true;

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseMoveCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
