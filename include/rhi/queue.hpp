#pragma once

#include <cstdint>
#include "rhi/structs.hpp"

namespace RHI {
class Queue {
public:
  Queue(vk::raii::Queue vkQueue, std::uint32_t familyIndex) : vkQueue {std::move(vkQueue)}, familyIndex {familyIndex} {

  };

  ~Queue() = default;

  Queue(const Queue&) = delete;
  auto operator=(Queue&) -> Queue& = delete;
  Queue(Queue&&) = delete;
  auto operator=(Queue&&) -> Queue&& = delete;

  [[nodiscard]] auto getFamilyIndex() const -> std::uint32_t {
    return familyIndex;
  }
private:
  vk::raii::Queue vkQueue;
  std::uint32_t familyIndex = 0;
};
}
