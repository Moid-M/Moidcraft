#pragma once
#include "core/types.hpp"
#include "core/input.hpp"
#include "ui/ui_renderer.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vulkan/vulkan.h>

enum class MenuAction {
    None,
    Singleplayer,
    Multiplayer,
    Options,
    Quit
};

class MenuState {
public:
    MenuState() = default;

    void init(UiRenderer* renderer, Input* input);
    void update(int fbW, int fbH, int winW, int winH);
    MenuAction action() const { return m_action; }
    void resetAction() { m_action = MenuAction::None; }
    void render(VkCommandBuffer cmd);

private:
    UiRenderer* m_renderer = nullptr;
    Input* m_input = nullptr;
    MenuAction m_action = MenuAction::None;

    struct Button {
        const char* label;
        float x, y, w, h;
        MenuAction clickAction;
    };

    Button m_buttons[6];
    int m_buttonCount = 0;
    int m_hoveredButton = -1;
    bool m_mouseWasDown = false;

    int m_fbW = 1280, m_fbH = 720;

    static constexpr float DESIGN_W = 1280.0f;
    static constexpr float DESIGN_H = 720.0f;

    float sx() const { return m_fbW / DESIGN_W; }
    float sy() const { return m_fbH / DESIGN_H; }

    void addButton(const char* label, float dy, MenuAction action);
};

