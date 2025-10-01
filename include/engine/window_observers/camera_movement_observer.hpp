#pragma once

#include "Camera.h"
#include "rhi/window.hpp"

namespace Engine {
class CameraMovementObserver : public RHI::WindowObserver {
public:
  CameraMovementObserver(Camera& camera) : camera(camera) {}

  void updateDeltaTime(float time) {
    deltaTime = time;
  }

  void onKeyPress(int key, int scancode, int action, int mods) override {
    if (key == GLFW_KEY_W && action == GLFW_PRESS) {
      camera.moveForward(deltaTime);
    }
    if (key == GLFW_KEY_S == GLFW_PRESS && action == GLFW_PRESS) {
      camera.moveBackward(deltaTime);
    }
    if (key == GLFW_KEY_A == GLFW_PRESS && action == GLFW_PRESS) {
      camera.moveLeft(deltaTime);
    }
    if (key == GLFW_KEY_D == GLFW_PRESS && action == GLFW_PRESS) {
      camera.moveRight(deltaTime);
    }
    if (key == GLFW_KEY_SPACE == GLFW_PRESS && action == GLFW_PRESS) {
      camera.moveUp(deltaTime);
    }
    if (key == GLFW_KEY_LEFT_SHIFT == GLFW_PRESS && action == GLFW_PRESS) {
      camera.moveDown(deltaTime);
    }
  }

  void onMouseMove(double xpos, double ypos) override {
    camera.handleMouseMovement(static_cast<float>(xpos), static_cast<float>(ypos));
  }
private:
  Camera& camera;
  float deltaTime = 0.0f;
};
}
