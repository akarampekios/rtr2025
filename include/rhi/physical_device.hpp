#pragma once

#include <map>
#include <ranges>
#include <vector>
#include <algorithm>

#include "rhi/instance.hpp"
#include "rhi/structs.hpp"
#include "rhi/window.hpp"

namespace RHI {
class PhysicalDevice {
public:
  struct Requirements {
    std::vector<std::string> requiredExtensions;
    std::vector<DeviceFeature> requiredFeatures;
    std::vector<QueueType> requiredQueueTypes;
  };

  PhysicalDevice(vk::raii::PhysicalDevice vkPhysicalDevice, const Window& window) : vkPhysicalDevice {std::move(vkPhysicalDevice)} {
    updateSupportedExtensions();
    updateSupportedQueueReferences(window);
  }

  ~PhysicalDevice() = default;
  PhysicalDevice(PhysicalDevice&&) = default;

  PhysicalDevice(const PhysicalDevice&) = delete;
  auto operator=(PhysicalDevice&) -> PhysicalDevice& = delete;
  auto operator=(PhysicalDevice&&) -> PhysicalDevice&& = delete;

  auto isSuitable(const Requirements& requirements) -> bool {
    return checkSupportedExtensions(requirements) && checkSupportedQueueTypes(requirements);
  }

  [[nodiscard]] auto getSupportedQueueTypes() const -> std::vector<QueueType> {
    std::vector<QueueType> queueTypes;

    for (const auto& queueType : supportedQueueReferences | std::views::keys) {
      queueTypes.emplace_back(queueType);
    }

    return queueTypes;
  }

  [[nodiscard]] auto getSupportedQueueReferences() const -> std::vector<QueueReference> {
    std::vector<QueueReference> queueReferences;

    for (const auto& queueReference : supportedQueueReferences | std::views::values) {
      queueReferences.emplace_back(queueReference);
    }

    return queueReferences;
  }

  [[nodiscard]] auto vkGetPhysicalDevice() const -> const vk::raii::PhysicalDevice& {
    return vkPhysicalDevice;
  }

  static auto listPhysicalDevices(const Instance& instance, const Window& window) -> std::vector<PhysicalDevice> {
    auto vkDevices = instance.vkGetInstance().enumeratePhysicalDevices();

    auto toPhysicalDevice = [&](const vk::raii::PhysicalDevice& device) {
      return PhysicalDevice(device, window);
    };

    auto physicalDevicesView = vkDevices | std::views::transform(toPhysicalDevice);

    std::vector physicalDevices(physicalDevicesView.begin(),
                                physicalDevicesView.end());

    return physicalDevices;
  }

  static auto findCompatiblePhysicalDevice(
    const Requirements& requirements,
    const Instance& instance,
    const Window& window
  ) -> PhysicalDevice {
    for (auto& device : listPhysicalDevices(instance, window)) {
      if (device.isSuitable(requirements)) {
        return std::move(device);
      }
    }

    throw InstanceException(InstanceException::ErrorCode::MISSING_COMPATIBLE_DEVICE);
  }
private:
  vk::raii::PhysicalDevice vkPhysicalDevice = nullptr;

  std::vector<std::string> supportedExtensions;

  std::map<QueueType, QueueReference> supportedQueueReferences;
  // std::vector<QueueReference> supportedQueueReferences;

  void updateSupportedExtensions() {
    supportedExtensions.clear();

    const auto vkExtensions = vkPhysicalDevice.enumerateDeviceExtensionProperties();
    for (const auto& extension : vkExtensions) {
      supportedExtensions.emplace_back(std::string(extension.extensionName.data()));
    }
  }

  // TODO: DRY
  void updateSupportedQueueReferences(const Window& window) {
    supportedQueueReferences.clear();

    const auto vkQueueFamilies = vkPhysicalDevice.getQueueFamilyProperties();

    // try to find dedicated transfer and compute queues
    bool foundDedicatedTransferQueue = false;
    bool foundDedicatedComputeQueue = false;

    for (std::uint32_t i = 0; i < vkQueueFamilies.size(); ++i) {
      if (
        (vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer) &&
        !(vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
        !(vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute)
      ) {
        if (!supportedQueueReferences.contains(QueueType::TRANSFER)) {
          supportedQueueReferences[QueueType::TRANSFER] = {
            .type = QueueType::TRANSFER,
            .familyIndex = i
          };
        }
        foundDedicatedTransferQueue = true;
      }
      if (
        (vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) &&
        !(vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
      ) {
        if (!supportedQueueReferences.contains(QueueType::COMPUTE)) {
          supportedQueueReferences[QueueType::COMPUTE] = {
            .type = QueueType::COMPUTE,
            .familyIndex = i
          };
        }
        foundDedicatedComputeQueue = true;
      }
    }

    for (std::uint32_t i = 0; i < vkQueueFamilies.size(); ++i) {
      if (static_cast<bool>(vkPhysicalDevice.getSurfaceSupportKHR(i, *window.vkGetSurface()))) {
        if (!supportedQueueReferences.contains(QueueType::PRESENTATION)) {
          supportedQueueReferences[QueueType::PRESENTATION] = {
            .type = QueueType::PRESENTATION,
            .familyIndex = i
          };
        }
      }
      if (vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
        if (!supportedQueueReferences.contains(QueueType::GRAPHICS)) {
          supportedQueueReferences[QueueType::GRAPHICS] = {
            .type = QueueType::GRAPHICS,
            .familyIndex = i
          };
        }
      }
      if ((vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) && !foundDedicatedComputeQueue) {
        if (!supportedQueueReferences.contains(QueueType::COMPUTE)) {
          supportedQueueReferences[QueueType::COMPUTE] = {
            .type = QueueType::COMPUTE,
            .familyIndex = i
          };
        }
      }
      if ((vkQueueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer) && !foundDedicatedTransferQueue) {
        if (!supportedQueueReferences.contains(QueueType::TRANSFER)) {
          supportedQueueReferences[QueueType::TRANSFER] = {
            .type = QueueType::TRANSFER,
            .familyIndex = i
          };
        }
      }
    }
  }

  auto checkSupportedExtensions(const Requirements& requirements) -> bool {
    std::vector<std::string> supportedExtensionsSorted = supportedExtensions;
    std::vector<std::string> requestedExtensionsSorted = requirements.requiredExtensions;

    std::ranges::sort(supportedExtensionsSorted);
    std::ranges::sort(requestedExtensionsSorted);

    return std::ranges::includes(supportedExtensionsSorted, requestedExtensionsSorted);
  }

  auto checkSupportedQueueTypes(const Requirements& requirements) -> bool {
    std::vector<QueueType> supportedQueueTypesSorted = getSupportedQueueTypes();
    std::vector<QueueType> requestedQueueTypesSorted = requirements.requiredQueueTypes;

    std::ranges::sort(supportedQueueTypesSorted);
    std::ranges::sort(requestedQueueTypesSorted);

    return std::ranges::includes(supportedQueueTypesSorted, requestedQueueTypesSorted);
  }
};
}
