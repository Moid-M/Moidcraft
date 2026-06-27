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
#include "ui/ui_renderer.hpp"
#include "ui/menu_state.hpp"
#include "ui/pause_state.hpp"
#include "ui/settings_state.hpp"
#include "core/settings.hpp"
#include <memory>

enum class AppState {
    Menu,
    Loading,
    Playing,
    Paused,
    Settings
};

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
    std::unique_ptr<UiRenderer> m_uiRenderer;
    std::unique_ptr<MenuState> m_menu;
    std::unique_ptr<PauseState> m_pauseMenu;
    std::unique_ptr<SettingsState> m_settings;

    std::string m_packPath;
    std::string m_settingsFilePath;
    GameSettings m_gameSettings;
    bool m_running = false;
    double m_lastFrame = 0.0;
    AppState m_state = AppState::Menu;
    float m_loadingProgress = 0.0f;
    AppState m_returnToState = AppState::Menu;

    bool m_showDebugOverlay = false;
    int m_fps = 0;
    int m_frameCount = 0;
    double m_fpsTimer = 0.0;
    double m_frameTimeMs = 0.0;
    static constexpr int m_frameTimeHistorySize = 1000;
    float m_frameTimeHistory[m_frameTimeHistorySize]{};
    int m_frameTimeIndex = 0;

    void init();
    void initWorld();
    void shutdown();
    void update(float dt);
    void render();
    void handleEvents();
    void renderDebugOverlay();
    void applySettings();
};
