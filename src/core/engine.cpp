#include "engine.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
namespace fs = std::filesystem;

Engine::Engine() = default;
Engine::~Engine() {
    m_gameSettings.save(m_settingsFilePath);
    shutdown();
}

void Engine::init() {
    m_settingsFilePath = "options.txt";
    m_gameSettings.load(m_settingsFilePath);

    m_window = std::make_unique<Window>(WIDTH, HEIGHT, "MoidCraft");

    m_input = std::make_unique<Input>();
    m_input->init(m_window->handle());
    m_input->setCursorMode(GLFW_CURSOR_NORMAL);

    m_camera = std::make_unique<Camera>();

    // Load texture pack
    m_texMgr = std::make_unique<TextureManager>();

    for (const auto& p : {
        ".", "..", "resourcepacks", "../run/resourcepacks"
    }) {
        if (!fs::is_directory(p)) continue;
        for (const auto& entry : fs::directory_iterator(p)) {
            if (entry.path().extension() == ".zip") {
                this->m_packPath = entry.path().string();
                break;
            }
        }
    }
    if (!m_packPath.empty() && m_texMgr->loadResourcePack(m_packPath)) {
        std::cout << "Resource pack loaded: " << m_packPath << std::endl;
        BlockDatabase::instance().resolveAtlasIndices(m_texMgr.get());
    } else {
        std::cout << "No resource pack found, generating procedural textures" << std::endl;
        m_texMgr->generateProceduralTextures();
    }

    // Create Vulkan context
    m_vkContext = std::make_unique<VulkanContext>();
    auto exts = m_window->getRequiredInstanceExtensions();
    m_vkContext->createInstance(exts);
    VkSurfaceKHR surface = m_window->createSurface(m_vkContext->instance());
    m_vkContext->init(exts, surface, WIDTH, HEIGHT);

    // Create texture on GPU
    m_texMgr->createVulkanTexture(
        m_vkContext->device(),
        m_vkContext->physicalDevice(),
        m_vkContext->commandPool(),
        m_vkContext->graphicsQueue());

    // Create renderer
    m_renderer = std::make_unique<VulkanRenderer>();
    m_renderer->setTextureManager(m_texMgr.get());
    m_renderer->init(m_vkContext.get(), m_camera.get());

    // Create UI renderer
    m_uiRenderer = std::make_unique<UiRenderer>();
    m_uiRenderer->init(m_vkContext.get(), "textures/ui");

    // Create menu
    m_menu = std::make_unique<MenuState>();
    m_menu->init(m_uiRenderer.get(), m_input.get());

    m_pauseMenu = std::make_unique<PauseState>();
    m_pauseMenu->init(m_uiRenderer.get(), m_input.get());

    m_settings = std::make_unique<SettingsState>();
    m_settings->init(m_uiRenderer.get(), m_input.get(), &m_gameSettings);

    m_renderer->setGamma(m_gameSettings.brightness);
    m_vkContext->setVSync(m_gameSettings.vsync);

    m_running = true;
    m_lastFrame = glfwGetTime();
    m_state = AppState::Menu;

    std::cout << "Engine initialized (menu)" << std::endl;
}

void Engine::initWorld() {
    std::cout << "Loading world..." << std::endl;

    // Create world
    m_world = std::make_unique<World>();
    m_world->init(m_vkContext.get(), m_texMgr.get());

    // Create player
    m_player = std::make_unique<Player>();
    m_player->init(m_camera.get(), m_input.get(), m_world.get());

    // Apply settings
    m_world->setRenderDistance(m_gameSettings.renderDistanceInt());
    m_player->setMouseSensitivity(m_gameSettings.mouseSensitivity);
    m_camera->setFov(m_gameSettings.fov);
    ChunkMesh::setTransparentLeaves(m_gameSettings.transparentLeaves);

    // Update world once to load initial chunks
    m_world->update(m_player->position());

    std::cout << "World loaded" << std::endl;
}

void Engine::shutdown() {
    m_running = false;
    m_player.reset();
    m_world.reset();
    m_menu.reset();
    m_uiRenderer.reset();
    if (m_renderer) m_renderer->cleanup();
    if (m_texMgr) m_texMgr->cleanupVulkan();
    if (m_vkContext) m_vkContext->cleanup();
}

void Engine::run() {
    try {
        init();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return;
    }
    m_input->update();

    while (m_running && !m_window->shouldClose()) {
        double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - m_lastFrame);
        m_lastFrame = currentTime;
        if (dt > 0.05f) dt = 0.05f;

        handleEvents();
        update(dt);
        render();
        m_input->update();

        double targetTime = 1.0 / std::max(m_gameSettings.fpsCapInt(), 10);
        double elapsed = glfwGetTime() - currentTime;
        if (elapsed < targetTime) {
            std::this_thread::sleep_for(std::chrono::duration<double>(targetTime - elapsed));
        }
    }
}

void Engine::handleEvents() {
    m_window->pollEvents();

    if (m_state == AppState::Menu) {
        m_menu->update(m_window->fbWidth(), m_window->fbHeight(), m_window->width(), m_window->height());
        MenuAction action = m_menu->action();
        if (action == MenuAction::Singleplayer) {
            m_renderer->waitIdle();
            m_state = AppState::Loading;
            m_menu->resetAction();
            initWorld();
            m_input->setCursorMode(GLFW_CURSOR_DISABLED);
            m_state = AppState::Playing;
            std::cout << "Entered gameplay" << std::endl;
        } else if (action == MenuAction::Options) {
            m_renderer->waitIdle();
            m_state = AppState::Settings;
            m_returnToState = AppState::Menu;
            m_menu->resetAction();
            m_settings->resetAction();
        } else if (action == MenuAction::Quit) {
            m_running = false;
        }
    }

    if (m_state == AppState::Paused) {
        m_pauseMenu->update(m_window->fbWidth(), m_window->fbHeight(), m_window->width(), m_window->height());
        PauseAction action = m_pauseMenu->action();
        if (action == PauseAction::Continue) {
            m_renderer->waitIdle();
            m_state = AppState::Playing;
            m_input->setCursorMode(GLFW_CURSOR_DISABLED);
            m_pauseMenu->resetAction();
        } else if (action == PauseAction::Options) {
            m_renderer->waitIdle();
            m_state = AppState::Settings;
            m_returnToState = AppState::Paused;
            m_pauseMenu->resetAction();
            m_settings->resetAction();
        } else if (action == PauseAction::ExitWorld) {
            m_renderer->waitIdle();
            m_pauseMenu->resetAction();
            m_player.reset();
            m_world.reset();
            m_state = AppState::Menu;
        }
        if (m_input->keyPressed(GLFW_KEY_ESCAPE)) {
            m_renderer->waitIdle();
            m_state = AppState::Playing;
            m_input->setCursorMode(GLFW_CURSOR_DISABLED);
            m_pauseMenu->resetAction();
        }
    }

    if (m_state == AppState::Settings) {
        m_settings->update(m_window->fbWidth(), m_window->fbHeight(), m_window->width(), m_window->height());
        SettingsAction action = m_settings->action();
        if (action == SettingsAction::Done || m_input->keyPressed(GLFW_KEY_ESCAPE)) {
            m_renderer->waitIdle();
            applySettings();
            m_gameSettings.save(m_settingsFilePath);
            m_state = m_returnToState;
            m_settings->resetAction();
        }
    }

    if (m_state == AppState::Playing) {
        if (m_input->keyPressed(GLFW_KEY_ESCAPE)) {
            m_renderer->waitIdle();
            m_state = AppState::Paused;
            m_input->setCursorMode(GLFW_CURSOR_NORMAL);
        }
        bool f3Down = m_input->keyDown(GLFW_KEY_F3);
        bool bPressed = m_input->keyPressed(GLFW_KEY_B);
        bool f3Pressed = m_input->keyPressed(GLFW_KEY_F3);
        if (m_world && f3Down && bPressed) {
            m_world->toggleDebugBorders();
        } else if (f3Pressed && !m_input->keyDown(GLFW_KEY_B) && !bPressed) {
            m_showDebugOverlay = !m_showDebugOverlay;
        }
    }
}

void Engine::update(float dt) {
    m_frameTimeMs = dt * 1000.0;
    m_frameTimeHistory[m_frameTimeIndex] = m_frameTimeMs;
    m_frameTimeIndex = (m_frameTimeIndex + 1) % m_frameTimeHistorySize;
    m_frameCount++;
    m_fpsTimer += dt;
    if (m_fpsTimer >= 1.0) {
        m_fps = m_frameCount;
        m_frameCount = 0;
        m_fpsTimer -= 1.0;
    }

    if (m_state == AppState::Playing || m_state == AppState::Paused) {
        if (m_player && m_world) {
            if (m_state == AppState::Playing) {
                m_player->update(dt);
            }
            m_world->update(m_player->position());
        }
    }
}

void Engine::render() {
    if (!m_renderer) return;

    try {
        m_renderer->beginFrame();

        if (m_state == AppState::Menu) {
            m_uiRenderer->beginFrame(m_window->fbWidth(), m_window->fbHeight());
            m_menu->render(m_renderer->currentCommandBuffer());
        } else if (m_state == AppState::Playing) {
            if (m_world) {
                m_renderer->setUnderwater(m_player && m_player->isInWater());
                m_world->render(m_renderer.get());
                m_world->renderTransparent(m_renderer.get());
                if (m_player) {
                    m_world->renderDebug(m_renderer.get(), m_player->position());
                }
            }
            if (m_showDebugOverlay) {
                m_uiRenderer->beginFrame(m_window->fbWidth(), m_window->fbHeight());
                renderDebugOverlay();
                m_uiRenderer->flushAll(m_renderer->currentCommandBuffer());
            }
        } else if (m_state == AppState::Paused) {
            if (m_world) {
                m_renderer->setUnderwater(m_player && m_player->isInWater());
                m_world->render(m_renderer.get());
                m_world->renderTransparent(m_renderer.get());
            }
            m_uiRenderer->beginFrame(m_window->fbWidth(), m_window->fbHeight());
            m_pauseMenu->render(m_renderer->currentCommandBuffer());
        } else if (m_state == AppState::Settings) {
            m_uiRenderer->beginFrame(m_window->fbWidth(), m_window->fbHeight());
            m_settings->render(m_renderer->currentCommandBuffer());
        }

        m_renderer->endFrame();
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()) == "Swapchain out of date") {
            m_renderer->waitIdle();
            if (m_window->fbWidth() > 0 && m_window->fbHeight() > 0) {
                m_vkContext->recreateSwapchain(m_window->fbWidth(), m_window->fbHeight());
            }
        } else {
            throw;
        }
    }
}

void Engine::renderDebugOverlay() {
    if (!m_player) return;
    auto pos = m_player->position();
    int cx = (int)std::floor(pos.x / CHUNK_SIZE);
    int cz = (int)std::floor(pos.z / CHUNK_SIZE);

    float size = 16.0f;
    float pad = 4.0f;
    float lineH = size + 6.0f;
    float rightX = (float)m_window->fbWidth() - 6.0f;
    float y = 10.0f;
    auto line = [&](const char* text, glm::vec4 color) {
        int w = m_uiRenderer->textWidth(text, size);
        float lx = rightX - (float)w;
        if (lx < 0) lx = 0;
        m_uiRenderer->drawRect(lx - pad, y - pad, (float)w + pad * 2, lineH, {0.0f, 0.0f, 0.0f, 0.6f});
        m_uiRenderer->drawTextFg(text, lx, y + 1, size, color);
        y += lineH + 2.0f;
    };

    // Compute 1% and 0.1% lows from frame time history
    int count = std::min(m_frameTimeIndex, m_frameTimeHistorySize);
    if (count > 0) {
        std::vector<float> times(m_frameTimeHistory, m_frameTimeHistory + count);
        std::sort(times.begin(), times.end());
        float low1ms = times[(int)(count * 0.01f)];
        float low01ms = times[(int)(count * 0.001f)];
        float low1fps = low1ms > 0 ? 1000.0f / low1ms : 0;
        float low01fps = low01ms > 0 ? 1000.0f / low01ms : 0;

        char buf[128];
        snprintf(buf, sizeof(buf), "FPS: %d  (%.1f ms)", m_fps, m_frameTimeMs);
        line(buf, {0.0f, 1.0f, 0.0f, 1.0f});

        snprintf(buf, sizeof(buf), "1%% low: %d (%.1f ms)", (int)low1fps, low1ms);
        line(buf, {0.6f, 1.0f, 0.6f, 1.0f});

        snprintf(buf, sizeof(buf), "0.1%% low: %d (%.1f ms)", (int)low01fps, low01ms);
        line(buf, {0.4f, 0.8f, 0.4f, 1.0f});

        snprintf(buf, sizeof(buf), "XYZ: %.1f / %.1f / %.1f", pos.x, pos.y, pos.z);
        line(buf, {1.0f, 1.0f, 1.0f, 1.0f});

        snprintf(buf, sizeof(buf), "Chunk: %d, %d  RD: %d", cx, cz, m_world ? m_world->renderDistance() : 0);
        line(buf, {1.0f, 1.0f, 1.0f, 1.0f});
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "FPS: %d  (%.1f ms)", m_fps, m_frameTimeMs);
        line(buf, {0.0f, 1.0f, 0.0f, 1.0f});

        snprintf(buf, sizeof(buf), "XYZ: %.1f / %.1f / %.1f", pos.x, pos.y, pos.z);
        line(buf, {1.0f, 1.0f, 1.0f, 1.0f});

        snprintf(buf, sizeof(buf), "Chunk: %d, %d  RD: %d", cx, cz, m_world ? m_world->renderDistance() : 0);
        line(buf, {1.0f, 1.0f, 1.0f, 1.0f});
    }
}

void Engine::applySettings() {
    m_renderer->setGamma(m_gameSettings.brightness);
    m_camera->setFov(m_gameSettings.fov);

    m_vkContext->setVSync(m_gameSettings.vsync);

    bool leavesChanged = (m_world && ChunkMesh::transparentLeavesEnabled() != m_gameSettings.transparentLeaves);
    ChunkMesh::setTransparentLeaves(m_gameSettings.transparentLeaves);

    if (m_world) {
        m_world->setRenderDistance(m_gameSettings.renderDistanceInt());
        if (leavesChanged) {
            m_world->rebuildAllMeshes();
        }
    }
    if (m_player) {
        m_player->setMouseSensitivity(m_gameSettings.mouseSensitivity);
    }
}
