#pragma once

#include <memory>

#include "rhi/window.hpp"
#include "engine/window_observers/default_observer.hpp"
#include "engine/window_observers/camera_movement_observer.hpp"

class SimpleRenderer;
class Camera;
class Scene;

class Application {
public:
    Application();
    ~Application();

    void run();
    void cleanup();

private:
    void initVulkan();
    void initWindowObservers();
    void mainLoop();
    void drawFrame();
    void update(float deltaTime);

    RHI::Window window;
    std::unique_ptr<SimpleRenderer> m_renderer;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Scene> m_scene;

    std::unique_ptr<Engine::CameraMovementObserver> cameraObserver = nullptr;
    std::unique_ptr<Engine::DefaultObserver> rootObserver = nullptr;

    bool m_running;
    float m_lastTime;
    float m_deltaTime;

    // Window properties
    static constexpr int WINDOW_WIDTH = 1920;
    static constexpr int WINDOW_HEIGHT = 1080;
    static constexpr const char* WINDOW_TITLE = "Cyberpunk City Demo";
};
