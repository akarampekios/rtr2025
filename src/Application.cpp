#include "Application.h"
#include "SimpleRenderer.h"
#include "Camera.h"
#include "Scene.h"
#include <iostream>
#include <chrono>
#include <thread>

#include "rhi/instance.hpp"
#include "rhi/window.hpp"
#include "engine/window_observers/default_observer.hpp"
#include "engine/window_observers/camera_movement_observer.hpp"

Application::Application()
    : m_running(false)
    , m_lastTime(0.0f)
    , m_deltaTime(0.0f) {
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    initWindow();
    initVulkan();
    initInputEvents();
    mainLoop();
}

void Application::initWindow() {
  window = std::make_unique<RHI::Window>();
}

void Application::initVulkan() {
    std::cout << "Creating renderer..." << std::endl;
    m_renderer = std::make_unique<SimpleRenderer>(&*window);
    
    std::cout << "Creating camera..." << std::endl;
    auto windowSize = window->getLogicalSize();
    m_camera = std::make_unique<Camera>(windowSize.width, windowSize.height);
    
    std::cout << "Creating scene..." << std::endl;
    m_scene = std::make_unique<Scene>();
    
    // Initialize scene with basic geometry
    std::cout << "Initializing scene..." << std::endl;
    m_scene->init();
    
    // Initialize renderer geometry with scene data
    std::cout << "Initializing renderer geometry..." << std::endl;
    m_renderer->initGeometry(m_scene.get());
    
    std::cout << "Vulkan initialization complete!" << std::endl;
}

void Application::initInputEvents() {
  rootObserver = std::make_unique<Engine::DefaultObserver>(*window);
  cameraObserver = std::make_unique<Engine::CameraMovementObserver>(*m_camera);

  window->addObserver("root", &*rootObserver);
  window->addObserver("camera", &*cameraObserver);
}

void Application::mainLoop() {
    m_running = true;
    m_lastTime = static_cast<float>(glfwGetTime());
    
    std::cout << "Starting main loop..." << std::endl;
    
    while (!window->shouldClose()) {
        window->pollEvents();
        
        float currentTime = static_cast<float>(glfwGetTime());
        m_deltaTime = currentTime - m_lastTime;
        m_lastTime = currentTime;
        
        update(m_deltaTime);
        drawFrame();
        
        // Add a small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    std::cout << "Main loop ended." << std::endl;
    vkDeviceWaitIdle(*m_renderer->getDevice().vkGetLogicalDevice());
}

void Application::drawFrame() {
    try {
        m_renderer->beginFrame();
        m_renderer->render(m_camera.get(), m_scene.get());
        m_renderer->endFrame();
    } catch (const std::exception& e) {
        std::cerr << "Error in drawFrame: " << e.what() << std::endl;
        m_running = false;
    }
}

void Application::update(float deltaTime) {
    cameraObserver->updateDeltaTime(deltaTime);
    m_camera->update(deltaTime);
    m_scene->update(deltaTime);
}


void Application::cleanup() {
    if (m_renderer) {
        m_renderer.reset();
    }
}
