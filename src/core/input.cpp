#include "input.hpp"

static Input* g_inputInstance = nullptr;

void Input::init(GLFWwindow* window) {
    m_window = window;
    g_inputInstance = this;
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

void Input::update() {
    m_keysPrev = m_keys;
    m_mouseBtnsPrev = m_mouseBtns;

    double x, y;
    glfwGetCursorPos(m_window, &x, &y);
    glm::vec2 newPos{(float)x, (float)y};

    if (m_firstMouse) {
        m_mousePos = newPos;
        m_firstMouse = false;
    }

    m_mouseDelta = newPos - m_mousePos;
    m_mousePos = newPos;
    m_scrollDelta = 0.0;
}

bool Input::keyPressed(int key) const {
    auto it = m_keys.find(key);
    auto itPrev = m_keysPrev.find(key);
    return (it != m_keys.end() && it->second) &&
           (itPrev == m_keysPrev.end() || !itPrev->second);
}

bool Input::keyDown(int key) const {
    auto it = m_keys.find(key);
    return it != m_keys.end() && it->second;
}

bool Input::keyReleased(int key) const {
    auto it = m_keys.find(key);
    auto itPrev = m_keysPrev.find(key);
    return (it == m_keys.end() || !it->second) &&
           (itPrev != m_keysPrev.end() && itPrev->second);
}

bool Input::mouseButtonPressed(int button) const {
    auto it = m_mouseBtns.find(button);
    auto itPrev = m_mouseBtnsPrev.find(button);
    return (it != m_mouseBtns.end() && it->second) &&
           (itPrev == m_mouseBtnsPrev.end() || !itPrev->second);
}

bool Input::mouseButtonDown(int button) const {
    auto it = m_mouseBtns.find(button);
    return it != m_mouseBtns.end() && it->second;
}

void Input::setCursorMode(int mode) {
    glfwSetInputMode(m_window, GLFW_CURSOR, mode);
}

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (g_inputInstance) {
        if (action == GLFW_PRESS) g_inputInstance->m_keys[key] = true;
        else if (action == GLFW_RELEASE) g_inputInstance->m_keys[key] = false;
    }
}

void Input::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (g_inputInstance) {
        if (action == GLFW_PRESS) g_inputInstance->m_mouseBtns[button] = true;
        else if (action == GLFW_RELEASE) g_inputInstance->m_mouseBtns[button] = false;
    }
}

void Input::mouseMoveCallback(GLFWwindow* window, double x, double y) {
}

void Input::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_inputInstance) {
        g_inputInstance->m_scrollDelta = yoffset;
    }
}
