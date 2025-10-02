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

enum class LogicalDeviceFeature : std::uint16_t {
  DYNAMIC_RENDERING = 1 << 0,
  EXTENDED_DYNAMIC_STATE = 1 << 1,
  SYNCHRONIZATION_2 = 1 << 2,
  SAMPLER_ANISOTROPY = 1 << 3,
  RAY_TRACING = 1 << 4,
  // ACCELERATION = 1 << 5,
  // DESCRIPTOR_INDEXING = 1 << 6,
  // BUFFER_DEVICE_ADDRESS = 1 << 7,
};
}
