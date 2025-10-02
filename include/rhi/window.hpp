#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <ranges>

#include "rhi/exception.hpp"
#include "rhi/instance.hpp"

namespace RHI {
  class WindowObserver {
  public:
    virtual void onKeyPress(int key, int scancode, int action, int mods) {};
    virtual void onMouseMove(double xpos, double ypos) {};
    virtual void onMouseClick(int button, int action, int mods) {};
  };

  class Window {
  public:
    static constexpr std::uint32_t DEFAULT_WIDTH = 1920;
    static constexpr std::uint32_t DEFAULT_HEIGHT = 1080;
    static inline const std::string DEFAULT_TITLE = "Cyberpunk City Demo";

    struct Settings {
      bool resizable = false;
      std::string title = DEFAULT_TITLE;
      vk::Extent2D size = {
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
      };
    };

    explicit Window() : Window({}) {}

    explicit Window(const Settings& settings) {
      createWindow(settings);
      hookObservers();
    }

    ~Window() {
      glfwDestroyWindow(glfWindow);
      glfwTerminate();
    }

    Window(const Window&) = delete;
    auto operator=(Window&) -> Window& = delete;
    Window(Window&&) = delete;
    auto operator=(Window&&) -> Window&& = delete;

    static auto getInstanceSurfaceExtensions() -> std::vector<std::string> {
      std::uint32_t glfwExtensionCount = 0;
      auto* const glfwExtensions =
          glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

      std::vector const extensions(glfwExtensions,
                                   glfwExtensions + glfwExtensionCount);  // NOLINT

      std::vector<std::string> requiredExtensions;
      requiredExtensions.reserve(glfwExtensionCount);

      for (const auto& extension : extensions) {
        requiredExtensions.emplace_back(extension);
      }

      return requiredExtensions;
    }

    void createSurface(const Instance& instance) {
      VkSurfaceKHR _surface = nullptr;

      if (glfwCreateWindowSurface(*instance.vkGetInstance(), glfWindow, nullptr, &_surface) != 0) {
        throw WindowException(WindowException::ErrorCode::SURFACE_CREATION_ERROR);
      }

      vkSurface = vk::raii::SurfaceKHR(instance.vkGetInstance(), _surface);
    }

    void addObserver(const std::string& name, WindowObserver* observer) {
      observers[name] = observer;
    }

    void removeObserver(const std::string& name) {
      observers.erase(name);
    }

    [[nodiscard]] auto getPixelSize() const -> vk::Extent2D {
      int width = 0;
      int height = 0;

      glfwGetFramebufferSize(glfWindow, &width, &height);

      return {
        .width = static_cast<std::uint32_t>(width),
        .height = static_cast<std::uint32_t>(height),
      };
    }

    [[nodiscard]] auto getLogicalSize() const -> vk::Extent2D {
      int width = 0;
      int height = 0;

      glfwGetWindowSize(glfWindow, &width, &height);

      return {
        .width = static_cast<std::uint32_t>(width),
        .height = static_cast<std::uint32_t>(height),
      };
    }

    [[nodiscard]] auto shouldClose() const -> bool {
      return glfwWindowShouldClose(glfWindow) == GLFW_TRUE || windowShouldClose;
    }

    void pollEvents() const {
      glfwPollEvents();
    }

    std::map<std::string, WindowObserver*>& getObservers() {
      return observers;
    }

    void close() {
      windowShouldClose = true;
    }

    void setCursorVisibility(const bool enabled) const {
      if (enabled) {
        glfwSetInputMode(glfWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      } else {
        glfwSetInputMode(glfWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      }
    }

    [[nodiscard]] auto vkGetSurface() const -> const vk::raii::SurfaceKHR& {
      return vkSurface;
    }
  private:
    std::map<std::string, WindowObserver*> observers;
    bool windowShouldClose = false;

    GLFWwindow* glfWindow = nullptr;
    vk::raii::SurfaceKHR vkSurface = nullptr;
    static bool glfwInitialized;

    void createWindow(const Settings& settings) {
      glfwInit();

      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

      if (!settings.resizable) {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
      }

      glfWindow = glfwCreateWindow(static_cast<int>(settings.size.width),
                                static_cast<int>(settings.size.height), settings.title.c_str(),
                                nullptr, nullptr);
    }

    void hookObservers() {
      glfwSetWindowUserPointer(glfWindow, this);

      glfwSetKeyCallback(glfWindow, [](GLFWwindow* glfwWindow, int key, int scancode, int action, int mods) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));

        for (const auto& observer : window->getObservers() | std::views::values) {
          observer->onKeyPress(key, scancode, action, mods);
        }
      });

      glfwSetCursorPosCallback(glfWindow, [](GLFWwindow* glfwWindow, double xpos, double ypos) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));

        for (const auto& observer : window->getObservers() | std::views::values) {
          observer->onMouseMove(xpos, ypos);
        }
      });

      glfwSetMouseButtonCallback(glfWindow, [](GLFWwindow* glfwWindow, int button, int action, int mods) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));

        for (const auto& observer : window->getObservers() | std::views::values) {
          observer->onMouseClick(button, action, mods);
        }
      });
    }
  };
}
