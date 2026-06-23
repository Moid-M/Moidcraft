#pragma once
#include "window.hpp"
#include "input.hpp"
#include "camera.hpp"
#include "core/types.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_renderer.hpp"
#include "world/world.hpp"
#include "player/player.hpp"
#include "core/texture_manager.hpp"
#include "world/block.hpp"
#include <memory>

class Engine {
public:
    Engine();
    ~Engine();

    void run();

private:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    std::unique_ptr<Window> m_window;
    std::unique_ptr<VulkanContext> m_vkContext;
    std::unique_ptr<VulkanRenderer> m_renderer;
    std::unique_ptr<Input> m_input;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<World> m_world;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<TextureManager> m_texMgr;

    bool m_running = false;
    double m_lastFrame = 0.0;

    void init();
    void shutdown();
    void update(float dt);
    void render();
    void handleEvents();
};
