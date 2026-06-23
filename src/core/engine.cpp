#include "engine.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

void Engine::init() {
    m_window = std::make_unique<Window>(WIDTH, HEIGHT, "Minecraft Clone");

    m_input = std::make_unique<Input>();
    m_input->init(m_window->handle());
    m_input->setCursorMode(GLFW_CURSOR_DISABLED);

    m_camera = std::make_unique<Camera>();

    // Load texture pack
    m_texMgr = std::make_unique<TextureManager>();
    std::string packPath;
    for (const auto& p : {
        ".", "..", "resourcepacks", "../run/resourcepacks"
    }) {
        if (!fs::is_directory(p)) continue;
        for (const auto& entry : fs::directory_iterator(p)) {
            if (entry.path().extension() == ".zip") {
                packPath = entry.path().string();
                break;
            }
        }
        if (!packPath.empty()) break;
    }
    if (!packPath.empty() && m_texMgr->loadResourcePack(packPath)) {
        std::cout << "Resource pack loaded: " << packPath << std::endl;
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

    // Create world
    m_world = std::make_unique<World>();
    m_world->init(m_vkContext.get(), m_texMgr.get());

    // Create player
    m_player = std::make_unique<Player>();
    m_player->init(m_camera.get(), m_input.get(), m_world.get());

    m_running = true;
    m_lastFrame = glfwGetTime();

    std::cout << "Engine initialized" << std::endl;
}

void Engine::shutdown() {
    m_running = false;
    if (m_renderer) m_renderer->cleanup();
    if (m_texMgr) m_texMgr->cleanupVulkan();
    if (m_vkContext) m_vkContext->cleanup();
}

void Engine::run() {
    init();
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
    }
}

void Engine::handleEvents() {
    m_window->pollEvents();

    if (m_input->keyPressed(GLFW_KEY_ESCAPE)) {
        static bool cursorVisible = false;
        cursorVisible = !cursorVisible;
        m_input->setCursorMode(cursorVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }
}

void Engine::update(float dt) {
    if (!m_player || !m_world) return;
    m_player->update(dt);
    m_world->update(m_player->position());
}

void Engine::render() {
    if (!m_renderer || !m_world) return;

    try {
        m_renderer->beginFrame();
        m_world->render(m_renderer.get());
        m_world->renderTransparent(m_renderer.get());
        m_renderer->endFrame();
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()) == "Swapchain out of date") {
            m_renderer->waitIdle();
            int w, h;
            glfwGetFramebufferSize(m_window->handle(), &w, &h);
            if (w > 0 && h > 0) {
                m_vkContext->recreateSwapchain(w, h);
            }
        } else {
            throw;
        }
    }
}
