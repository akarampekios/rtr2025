#pragma once

#include <cstdint>
#include <memory>
#include <map>
#include <unordered_set>

#include "rhi/structs.hpp"
#include "rhi/queue.hpp"
#include "rhi/physical_device.hpp"
#include "utils/string_utils.hpp"

namespace RHI {
class LogicalDevice {
public:
  struct Settings {
    std::vector<std::string> deviceExtensions;
    std::vector<LogicalDeviceFeature> deviceFeatures;
    std::vector<QueueType> queueTypes;
  };

  LogicalDevice(const Settings& settings, const PhysicalDevice& physicalDevice) {
    createLogicalDevice(settings, physicalDevice);
    createQueueHandlers();
  }

  ~LogicalDevice() = default;

  LogicalDevice(const LogicalDevice&) = delete;
  auto operator=(LogicalDevice&) -> LogicalDevice& = delete;
  LogicalDevice(LogicalDevice&&) = delete;
  auto operator=(LogicalDevice&&) -> LogicalDevice&& = delete;

  [[nodiscard]] auto getQueue(const QueueType queueType) const -> const Queue& {
    return *queues.at(queueType);
  }

  [[nodiscard]] auto vkGetLogicalDevice() const -> const vk::raii::Device& {
    return *vkLogicalDevice;
  }
private:
  std::unique_ptr<vk::raii::Device> vkLogicalDevice = nullptr;
  std::map<QueueType, std::unique_ptr<Queue>> queues;
  std::map<std::uint32_t, std::vector<QueueType>> queueIndexToTypeMap;

  void createLogicalDevice(const Settings& settings, const PhysicalDevice& physicalDevice) {
    auto featureChain = buildFeatures(settings);
    auto queueCreateInfos = buildQueueInfos(settings, physicalDevice);
    auto extensionNames = StringUtils::toCStrings(settings.deviceExtensions);

    vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount =
          static_cast<std::uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
      .ppEnabledExtensionNames = extensionNames.data(),
    };

    vkLogicalDevice = std::make_unique<vk::raii::Device>(
      physicalDevice.vkGetPhysicalDevice(), deviceCreateInfo);
  }

  void createQueueHandlers() {
    for (const auto& [queueIndex, queueTypes] : queueIndexToTypeMap) {
      for (const auto& queueType : queueTypes) {
        auto vkQueue = vk::raii::Queue(*vkLogicalDevice, queueIndex, 0);
        queues[queueType] = std::make_unique<Queue>(vkQueue, queueIndex);
      }
    }
  }

  auto buildFeatures(const Settings& settings)->
  vk::StructureChain<
    vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
    vk::PhysicalDeviceBufferDeviceAddressFeatures,
    vk::PhysicalDeviceDescriptorIndexingFeatures,
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> {

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedStateFeatures{};
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatures{};
    vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};

    for (const auto& feature : settings.deviceFeatures) {
      if (feature == LogicalDeviceFeature::DYNAMIC_RENDERING) {
        vulkan13Features.dynamicRendering = vk::True;
      }
      if (feature == LogicalDeviceFeature::EXTENDED_DYNAMIC_STATE) {
        extendedStateFeatures.extendedDynamicState = vk::True;
      }
      if (feature == LogicalDeviceFeature::SYNCHRONIZATION_2) {
        vulkan13Features.synchronization2 = vk::True;
      }
      if (feature == LogicalDeviceFeature::SAMPLER_ANISOTROPY) {
        deviceFeatures2.features.samplerAnisotropy = vk::True;
      }
      if (feature == LogicalDeviceFeature::RAY_TRACING) {
        bufferAddressFeatures.bufferDeviceAddress = vk::True;
        descriptorIndexing.shaderSampledImageArrayNonUniformIndexing = vk::True;
        descriptorIndexing.descriptorBindingPartiallyBound = vk::True;
        descriptorIndexing.descriptorBindingVariableDescriptorCount = vk::True;
        descriptorIndexing.runtimeDescriptorArray = vk::True;
        accelFeatures.accelerationStructure = vk::True;
        rayTracingPipelineFeatures.rayTracingPipeline = vk::True;
      }
    }

    return {
      deviceFeatures2,
      vulkan13Features,
      extendedStateFeatures,
      bufferAddressFeatures,
      descriptorIndexing,
      accelFeatures,
      rayTracingPipelineFeatures
    };
  }

  auto buildQueueInfos(const Settings& settings, const PhysicalDevice& physicalDevice) -> std::vector<vk::DeviceQueueCreateInfo> {
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::unordered_set<std::uint32_t> acquiredFamilyIndexes;

    for (const auto& requestedQueueType : settings.queueTypes) {
      for (const auto& [queueType, queueIndex] : physicalDevice.getSupportedQueueReferences()) {
        if (requestedQueueType != queueType) {
          continue;
        }

        queueIndexToTypeMap[queueIndex].emplace_back(queueType);

        if (acquiredFamilyIndexes.contains(queueIndex)) {
          continue;
        }

        float constexpr queuePriority = 0.0F;
        vk::DeviceQueueCreateInfo const deviceQueueCreateInfo{
          .queueFamilyIndex = queueIndex,
          .queueCount = 1,
          .pQueuePriorities = &queuePriority,
        };

        acquiredFamilyIndexes.insert(queueIndex);
        queueCreateInfos.emplace_back(deviceQueueCreateInfo);
      }
    }

    return queueCreateInfos;
  }
};
}
