#pragma once

#include <cstdint>

namespace RHI {
enum class QueueType : std::uint8_t {
  GRAPHICS = 1 << 0,
  PRESENTATION = 1 << 1,
  TRANSFER = 1 << 2,
  COMPUTE = 1 << 3,
};

struct QueueReference {
  QueueType type;
  std::uint32_t familyIndex;
};

enum class DeviceFeature : std::uint8_t {
  GRAPHICS = 1 << 0,
  PRESENTATION = 1 << 1,
  TRANSFER = 1 << 2,
  COMPUTE = 1 << 3,
};
}
