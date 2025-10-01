#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <cstdint>

#include "rhi/exception.hpp"
#include "utils/string_utils.hpp"

namespace RHI {
VKAPI_ATTR auto VKAPI_CALL
instanceDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT /*severity*/,
              const vk::DebugUtilsMessageTypeFlagsEXT type,
              const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* /*unused*/) -> vk::Bool32 {
  std::cerr << std::format("[{}]: {}", to_string(type), pCallbackData->pMessage)
            << '\n';

  return vk::False;
}

class Instance {
public:
  static constexpr std::uint32_t DEFAULT_VULKAN_VERSION = VK_API_VERSION_1_4;
  static constexpr std::string_view DEFAULT_APP_NAME = "Vulkan Application";
  static constexpr std::uint32_t DEFAULT_APP_VERSION = VK_MAKE_VERSION(1, 0, 0);

  struct Settings {
    bool enableDebugUtilities = true;
    std::vector<std::string> surfaceExtensions;
    std::uint32_t apiVersion = DEFAULT_VULKAN_VERSION;
    std::string applicationName = std::string(DEFAULT_APP_NAME);
    std::uint32_t applicationVersion = DEFAULT_APP_VERSION;
    std::string engineName = std::string(DEFAULT_APP_NAME);
    std::uint32_t engineVersion = DEFAULT_APP_VERSION;
  };

  explicit Instance(const Settings& settings) {
    createInstance(settings);

    if (settings.enableDebugUtilities) {
      setupDebugMessenger();
    }
  }

  ~Instance() = default;

  Instance(const Instance&) = delete;
  auto operator=(Instance&) -> Instance& = delete;
  Instance(Instance&&) = delete;
  auto operator=(Instance&&) -> Instance&& = delete;

  [[nodiscard]] auto vkGetInstance() const
    -> const vk::raii::Instance& {
    return vkInstance;
  }

private:
  vk::raii::Context vkContext;
  vk::raii::Instance vkInstance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

  void createInstance(const Settings& settings) {
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> validationLayers;

    for (const auto& extension : settings.surfaceExtensions) {
      instanceExtensions.emplace_back(extension);
    }

    if (settings.enableDebugUtilities) {
      instanceExtensions.emplace_back(vk::EXTDebugUtilsExtensionName);
      validationLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    }

    validateRequiredExtensions(instanceExtensions);
    validateRequiredLayers(validationLayers);

    auto vkLayers = StringUtils::toCStrings(validationLayers);
    auto vkExtensions = StringUtils::toCStrings(instanceExtensions);

    vk::ApplicationInfo const appInfo{
      .pApplicationName = settings.applicationName.c_str(),
      .applicationVersion = settings.applicationVersion,
      .pEngineName = settings.engineName.c_str(),
      .engineVersion = settings.engineVersion,
      .apiVersion = settings.apiVersion,
    };

    vk::InstanceCreateInfo const createInfo{
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(vkLayers.size()),
      .ppEnabledLayerNames = vkLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(vkExtensions.size()),
      .ppEnabledExtensionNames = vkExtensions.data(),
    };

    vkInstance = vk::raii::Instance(vkContext, createInfo);
  }

  void validateRequiredExtensions(const std::vector<std::string>& requiredExtensions) const {
    auto extensionProperties = vkContext.enumerateInstanceExtensionProperties();

    for (auto const& extension : requiredExtensions) {
      auto extensionCheck =
          [&](const vk::ExtensionProperties& extensionProperty) {
            return std::string(extensionProperty.extensionName.data()) == extension;
      };
      if (std::ranges::none_of(extensionProperties, extensionCheck)) {
        throw InstanceException(
            InstanceException::ErrorCode::MISSING_REQUIRED_EXTENSION, extension);
      }
    }
  }

  void validateRequiredLayers(const std::vector<std::string>& requiredLayers) const {
    auto layerProperties = vkContext.enumerateInstanceLayerProperties();

    for (auto const& layer : requiredLayers) {
      auto layerCheck = [&](auto const& layerProperty) {
        return std::string(layerProperty.layerName.data()) == layer;
      };
      if (std::ranges::none_of(layerProperties, layerCheck)) {
        throw InstanceException(
            InstanceException::ErrorCode::MISSING_REQUIRED_EXTENSION, layer);
      }
    }
  }

  void setupDebugMessenger() {
    constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        // vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    constexpr vk::DebugUtilsMessengerCreateInfoEXT
        debugUtilsMessengerCreateInfoEXT{
          .messageSeverity = severityFlags,
          .messageType = messageTypeFlags,
          .pfnUserCallback = &instanceDebugCallback,
      };

    debugMessenger =
        vkInstance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }
};
}
