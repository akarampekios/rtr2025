#pragma once

#include "rhi/window.hpp"

namespace Engine {
class DefaultObserver : public RHI::WindowObserver {
public:
  DefaultObserver(RHI::Window& window) : window(window) {}

  void onKeyPress(int key, int scancode, int action, int mods) override {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      window.close();
    }
  }
private:
  RHI::Window& window;
};
}
