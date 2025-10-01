#include "SimpleRenderer.h"
#include "Camera.h"
#include "Scene.h"
#include "ShaderManager.h"
#include "GLTFLoader.h"

#include "rhi/structs.hpp"
#include "rhi/instance.hpp"
#include "rhi/physical_device.hpp"

// #if defined(_WIN32)
// #ifndef NOMINMAX
// #define NOMINMAX
// #endif
// #include <windows.h>
// #include <vulkan/vulkan_win32.h>
// #endif

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

static const std::vector<std::string> PHYSICAL_DEVICE_EXTENSIONS = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
  VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
  VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
  VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
  VK_KHR_SPIRV_1_4_EXTENSION_NAME,
  VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
  VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
  VK_KHR_MAINTENANCE3_EXTENSION_NAME
};

static const std::vector DEVICE_QUEUE_TYPES = {
  RHI::QueueType::GRAPHICS,
  RHI::QueueType::PRESENTATION,
  RHI::QueueType::TRANSFER,
  RHI::QueueType::COMPUTE,
};


namespace {
VkTransformMatrixKHR makeIdentityTransformMatrix() {
    VkTransformMatrixKHR matrix{};
    matrix.matrix[0][0] = 1.0f;
    matrix.matrix[1][1] = 1.0f;
    matrix.matrix[2][2] = 1.0f;
    return matrix;
}
} // namespace

namespace {
const char* layoutToString(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
    case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY";
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY";
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC";
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST";
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED";
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT";
    default: return "UNKNOWN";
    }
}
} // namespace

bool SimpleRenderer::findQueueFamilies(VkPhysicalDevice device, uint32_t& graphicsFamily, uint32_t& presentFamily) {
    graphicsFamily = UINT32_MAX;
    presentFamily = UINT32_MAX;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        const auto& queueFamily = queueFamilies[i];
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphicsFamily == UINT32_MAX) {
            graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        if (m_surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        }
        if (presentSupport && presentFamily == UINT32_MAX) {
            presentFamily = i;
        }

        if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
            return true;
        }
    }

    return false;
}

VkDeviceAddress SimpleRenderer::getBufferDeviceAddress(VkBuffer buffer) const {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;
    return vkGetBufferDeviceAddress(m_device, &addressInfo);
}

SimpleRenderer::SimpleRenderer(RHI::Window* window)
    : m_window(window)
    , m_graphicsQueueFamilyIndex(UINT32_MAX)
    , m_presentQueueFamilyIndex(UINT32_MAX)
    , m_currentFrame(0)
    , m_imageIndex(0)
    , m_vertexBuffer(VK_NULL_HANDLE)
    , m_vertexBufferMemory(VK_NULL_HANDLE)
    , m_indexBuffer(VK_NULL_HANDLE)
    , m_indexBufferMemory(VK_NULL_HANDLE)
    , m_vertexCount(0)
    , m_indexCount(0)
    , m_instancesBuffer(VK_NULL_HANDLE)
    , m_instancesMemory(VK_NULL_HANDLE)
    , m_rtStorageImage(VK_NULL_HANDLE)
    , m_rtStorageMemory(VK_NULL_HANDLE)
    , m_rtStorageImageView(VK_NULL_HANDLE)
    , m_rtStorageFormat(VK_FORMAT_UNDEFINED)
    , m_rtDescriptorSetLayout(VK_NULL_HANDLE)
    , m_rtPipelineLayout(VK_NULL_HANDLE)
    , m_rtPipeline(VK_NULL_HANDLE)
    , m_rtDescriptorPool(VK_NULL_HANDLE)
    , m_rtShaderBindingTable(VK_NULL_HANDLE)
    , m_rtShaderBindingTableMemory(VK_NULL_HANDLE)
    , m_rtReady(false) {
    initVulkan();
}

SimpleRenderer::~SimpleRenderer() {
    cleanupRayTracingPipeline();
    cleanupRayTracingStorageImage();
    cleanupAccelerationStructures();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
        vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
        vkDestroyBuffer(m_device, m_lightingBuffers[i], nullptr);
        vkFreeMemory(m_device, m_lightingBuffersMemory[i], nullptr);
    }
    
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    
    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    
    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    vkDestroyDevice(m_device, nullptr);

    // NOTE these are handled by RAII now
    // vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    // vkDestroyInstance(m_instance, nullptr);
}

// FIXME: Switch to RAII and dynamic rendering to simplify the setup
void SimpleRenderer::initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    // FIXME: switch to dynamic rendering
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
    createUniformBuffers();
    createLightingBuffers();
    createDescriptorPool();
    createDescriptorSets();
    loadRayTracingFunctions();
    createAccelerationStructures();
}

void SimpleRenderer::createInstance() {
    RHI::Instance::Settings instanceSettings {
      .enableDebugUtilities = true,
      .surfaceExtensions = RHI::Window::getInstanceSurfaceExtensions(),
    };

    instance = std::make_unique<RHI::Instance>(instanceSettings);

    // FIXME: for backward compatibility, there is no need to mess with the raw vk pointer anymore
    m_instance = *instance->vkGetInstance();
}

void SimpleRenderer::createSurface() {
    m_window->createSurface(*instance);
    m_surface = *m_window->vkGetSurface();
}

void SimpleRenderer::pickPhysicalDevice() {
  RHI::PhysicalDevice::Requirements const physicalDeviceRequirements {
    .requiredExtensions = PHYSICAL_DEVICE_EXTENSIONS,
    .requiredQueueTypes = DEVICE_QUEUE_TYPES,
  };

  physicalDevice = std::make_unique<RHI::PhysicalDevice>(
    RHI::PhysicalDevice::findCompatiblePhysicalDevice(
      physicalDeviceRequirements,
      *instance,
      *m_window
    )
  );

  // FIXME: for backward compatibility, there is no need to mess with the raw vk pointer anymore
  m_physicalDevice = *physicalDevice->vkGetPhysicalDevice();
}

void SimpleRenderer::createLogicalDevice() {
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;
    if (!findQueueFamilies(m_physicalDevice, graphicsFamily, presentFamily)) {
        throw std::runtime_error("failed to find required queue families!");
    }

    m_graphicsQueueFamilyIndex = graphicsFamily;
    m_presentQueueFamilyIndex = presentFamily;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatures{};
    bufferAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexing.runtimeDescriptorArray = VK_TRUE;
    descriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
    descriptorIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptorIndexing.pNext = &bufferAddressFeatures;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
    accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelFeatures.accelerationStructure = VK_TRUE;
    accelFeatures.pNext = &descriptorIndexing;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rtPipelineFeatures.pNext = &accelFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &rtPipelineFeatures;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(PHYSICAL_DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = PHYSICAL_DEVICE_EXTENSIONS.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(m_device, graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, presentFamily, 0, &m_presentQueue);
}

void SimpleRenderer::createSwapChain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& candidate : formats) {
        if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = candidate;
            break;
        }
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end()) {
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(static_cast<uint32_t>(1920), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(1080), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {m_graphicsQueueFamilyIndex, m_presentQueueFamilyIndex};
    if (m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void SimpleRenderer::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());
    
    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void SimpleRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void SimpleRenderer::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    
    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            m_swapChainImageViews[i]
        };
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void SimpleRenderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0; // Simplified
    
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void SimpleRenderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
    
    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void SimpleRenderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void SimpleRenderer::beginFrame() {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);
    
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
}

void SimpleRenderer::render(Camera* camera, Scene* scene) {
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    updateUniformBuffer(m_currentFrame, camera);
    updateLightingBuffer(m_currentFrame);
    static bool loggedNotReady = false;
    if (!m_rtReady && !loggedNotReady) {
        std::cout << "[RT] Resources not ready for ray tracing dispatch" << std::endl;
        loggedNotReady = true;
    }

    // Start render pass for hybrid rendering
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[m_imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;
    
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    
    vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (m_rtReady && m_rtPipeline != VK_NULL_HANDLE && m_topLevelAS.handle != VK_NULL_HANDLE && m_rtStorageImageView != VK_NULL_HANDLE) {
        loggedNotReady = false;
        static int frameCount = 0;
        frameCount++;

        static const bool debugCopyEnabled = [] {
            const char* env = std::getenv("DEBUG_RT_COPY");
            return env && env[0] != '\0' && env[0] != '0';
        }();

        if (debugCopyEnabled) {
            std::cout << "[RT][Debug] DEBUG_RT_COPY enabled - skipping ray dispatch" << std::endl;
            vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
            VkImage swapchainImage = m_swapChainImages[m_imageIndex];
            transitionImageLayout(swapchainImage,
                                  m_swapChainImageFormat,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT);
            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.dstSubresource = copyRegion.srcSubresource;
            copyRegion.extent = {m_swapChainExtent.width, m_swapChainExtent.height, 1};
            std::cout << "[RT][Debug] Copying storage image 0x" << std::hex << reinterpret_cast<uint64_t>(m_rtStorageImage)
                      << " to swapchain image 0x" << reinterpret_cast<uint64_t>(swapchainImage) << std::dec << std::endl;
            vkCmdCopyImage(m_commandBuffers[m_currentFrame],
                           m_rtStorageImage, VK_IMAGE_LAYOUT_GENERAL,
                           swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copyRegion);
            transitionImageLayout(swapchainImage,
                                  m_swapChainImageFormat,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT,
                                  0);
            std::cout << "[RT][Debug] Completed debug copy path" << std::endl;
        }

        if (!debugCopyEnabled && frameCount % 60 == 0) { // Log every 60 frames to avoid spam
            std::cout << "[RT] Frame " << frameCount << " - Dispatching rays for main scene" << std::endl;
            std::cout << "[RT] Debug - Pipeline: " << (m_rtPipeline != VK_NULL_HANDLE ? "OK" : "NULL") << std::endl;
            std::cout << "[RT] Debug - TLAS: " << (m_topLevelAS.handle != VK_NULL_HANDLE ? "OK" : "NULL") << std::endl;
            std::cout << "[RT] Debug - Storage Image: " << (m_rtStorageImageView != VK_NULL_HANDLE ? "OK" : "NULL") << std::endl;
            std::cout << "[RT] Debug - Descriptor Set: " << (m_rtDescriptorSets[m_currentFrame] != VK_NULL_HANDLE ? "OK" : "NULL") << std::endl;
            std::cout << "[RT] Debug - Raygen Region Address: " << m_rtRaygenRegion.deviceAddress << std::endl;
            std::cout << "[RT] Debug - Miss Region Address: " << m_rtMissRegion.deviceAddress << std::endl;
            std::cout << "[RT] Debug - Hit Region Address: " << m_rtHitRegion.deviceAddress << std::endl;
        }
        
        // Dispatch ray tracing to storage image
        VkDescriptorSet rtSet = m_rtDescriptorSets[m_currentFrame];

        static bool loggedBindings = false;
        if (!loggedBindings) {
            std::cout << "[RT][Debug] Binding pipeline for frame " << m_currentFrame << std::endl;
            std::cout << "[RT][Debug] Descriptor set handle: 0x" << std::hex << reinterpret_cast<uint64_t>(rtSet) << std::dec << std::endl;
            std::cout << "[RT][Debug] Storage image view: 0x" << std::hex << reinterpret_cast<uint64_t>(m_rtStorageImageView) << std::dec << std::endl;
            std::cout << "[RT][Debug] Storage image: 0x" << std::hex << reinterpret_cast<uint64_t>(m_rtStorageImage) << std::dec << std::endl;
            std::cout << "[RT][Debug] TLAS address: 0x" << std::hex << m_topLevelAS.deviceAddress << std::dec << std::endl;
            loggedBindings = true;
        }

        if (!debugCopyEnabled) {
            vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
            vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0, 1, &rtSet, 0, nullptr);
            m_vkCmdTraceRaysKHR(m_commandBuffers[m_currentFrame], &m_rtRaygenRegion, &m_rtMissRegion, &m_rtHitRegion, &m_rtCallableRegion, m_swapChainExtent.width, m_swapChainExtent.height, 1);
            std::cout << "[RT][Debug] Dispatched rays: " << m_swapChainExtent.width << "x" << m_swapChainExtent.height << std::endl;
        }

        VkImageMemoryBarrier storageBarrier{};
        storageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        storageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        storageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        storageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        storageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        storageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        storageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        storageBarrier.image = m_rtStorageImage;
        storageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        storageBarrier.subresourceRange.baseMipLevel = 0;
        storageBarrier.subresourceRange.levelCount = 1;
        storageBarrier.subresourceRange.baseArrayLayer = 0;
        storageBarrier.subresourceRange.layerCount = 1;
        if (!debugCopyEnabled) {
            vkCmdPipelineBarrier(m_commandBuffers[m_currentFrame],
                                 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &storageBarrier);
            std::cout << "[RT][Debug] Barrier for storage image before blit" << std::endl;
        }
        
        // End render pass before blitting
        std::cout << "[RT][Debug] End ray tracing render pass" << std::endl;
        vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
        
        // Blit ray traced result to swapchain
        VkImage swapchainImage = m_swapChainImages[m_imageIndex];
        
        if (frameCount % 60 == 0) {
            std::cout << "[RT] Debug - Blitting from storage image to swapchain" << std::endl;
            std::cout << "[RT] Debug - Swapchain Image: " << (swapchainImage != VK_NULL_HANDLE ? "OK" : "NULL") << std::endl;
            std::cout << "[RT] Debug - Storage Image: " << (m_rtStorageImage != VK_NULL_HANDLE ? "OK" : "NULL") << std::endl;
            std::cout << "[RT] Debug - Image Extent: " << m_swapChainExtent.width << "x" << m_swapChainExtent.height << std::endl;
        }
        
        transitionImageLayout(swapchainImage,
                              m_swapChainImageFormat,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT);

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = 0;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1] = {static_cast<int32_t>(m_swapChainExtent.width), static_cast<int32_t>(m_swapChainExtent.height), 1};

        blit.dstSubresource = blit.srcSubresource;
        blit.dstOffsets[1] = blit.srcOffsets[1];

        if (!debugCopyEnabled) {
            vkCmdBlitImage(m_commandBuffers[m_currentFrame],
                           m_rtStorageImage, VK_IMAGE_LAYOUT_GENERAL,
                           swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit,
                           VK_FILTER_LINEAR);
            std::cout << "[RT][Debug] Issued blit from storage image 0x" << std::hex << reinterpret_cast<uint64_t>(m_rtStorageImage)
                      << " (layout " << layoutToString(VK_IMAGE_LAYOUT_GENERAL) << ") to swapchain image 0x"
                      << reinterpret_cast<uint64_t>(swapchainImage) << " (layout " << layoutToString(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) << ")"
                      << std::dec << std::endl;

            transitionImageLayout(swapchainImage,
                                  m_swapChainImageFormat,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT,
                                  0);
            std::cout << "[RT][Debug] Restored swapchain image to " << layoutToString(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) << std::endl;
        }
    } else {
        // Fallback to raster rendering if ray tracing not ready
        if (m_rtReady) {
            loggedNotReady = false;
            std::cout << "[RT] Skip ray tracing dispatch (resources not ready), using raster fallback" << std::endl;
        }
        
        // Render using traditional rasterization
        vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        
        VkBuffer vertexBuffers[] = {m_vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(m_commandBuffers[m_currentFrame], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(m_commandBuffers[m_currentFrame], m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        
        VkDescriptorSet descriptorSet = m_descriptorSets[m_currentFrame];
        vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        
        vkCmdDrawIndexed(m_commandBuffers[m_currentFrame], m_indexCount, 1, 0, 0, 0);
        
        std::cout << "[RT][Debug] End raster render pass" << std::endl;
        vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
    }

    if (vkEndCommandBuffer(m_commandBuffers[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void SimpleRenderer::endFrame() {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
submit_frame:
    std::cout << "[RT][Debug] Submitting command buffer for frame " << m_currentFrame << std::endl;
    VkResult submitResult = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);
    std::cout << "[RT][Debug] vkQueueSubmit result: " << submitResult << std::endl;
    if (submitResult != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {m_swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_imageIndex;
    
    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    std::cout << "[RT][Debug] Presented image index " << m_imageIndex << " result: " << result << std::endl;
    
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }
    
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void SimpleRenderer::createVertexBuffer(const std::vector<GLTFVertex>& vertices) {
    if (vertices.empty()) {
        // Fallback to a simple triangle if no vertices provided
        std::vector<GLTFVertex> fallbackVertices = {
            {{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
        };
        createVertexBufferFromData(fallbackVertices);
        return;
    }
    
    createVertexBufferFromData(vertices);
}

void SimpleRenderer::createVertexBufferFromData(const std::vector<GLTFVertex>& vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_device, stagingBufferMemory);
    
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_vertexBuffer,
                 m_vertexBufferMemory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
    
    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
    
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void SimpleRenderer::createIndexBuffer(const std::vector<uint32_t>& indices) {
    if (indices.empty()) {
        // Fallback to a simple triangle if no indices provided
        std::vector<uint32_t> fallbackIndices = {0, 1, 2};
        createIndexBufferFromData(fallbackIndices);
        return;
    }
    
    createIndexBufferFromData(indices);
}

void SimpleRenderer::initGeometry(Scene* scene) {
    if (scene) {
        const auto& vertices = scene->getVertices();
        const auto& indices = scene->getIndices();
        
        std::cout << "Initializing geometry with " << vertices.size() << " vertices and " << indices.size() << " indices" << std::endl;
        
        // Debug: Print first few vertices to check if they're reasonable
        if (!vertices.empty()) {
            std::cout << "First vertex: pos(" << vertices[0].position.x << ", " << vertices[0].position.y << ", " << vertices[0].position.z << ")" << std::endl;
            std::cout << "Last vertex: pos(" << vertices.back().position.x << ", " << vertices.back().position.y << ", " << vertices.back().position.z << ")" << std::endl;
        }
        
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());
    createAccelerationStructures();
        
    } else {
        // Fallback to triangle
    createVertexBuffer(std::vector<GLTFVertex>());
    createIndexBuffer(std::vector<uint32_t>());
    m_vertexCount = 3;
    m_indexCount = 3;
    createAccelerationStructures();
    }
}

void SimpleRenderer::createIndexBufferFromData(const std::vector<uint32_t>& indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_device, stagingBufferMemory);
    
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_indexBuffer,
                 m_indexBufferMemory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
    
    copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
    
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void SimpleRenderer::createGraphicsPipeline() {
    // Load shader modules
    VkShaderModule vertShaderModule = ShaderManager::loadShader(m_device, "vert.spv");
    VkShaderModule fragShaderModule = ShaderManager::loadShader(m_device, "frag.spv");
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input state
    auto bindingDescription = GLTFVertex::getBindingDescription();
    auto attributeDescriptions = GLTFVertex::getAttributeDescriptions();
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swapChainExtent.width;
    viewport.height = (float)m_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Color blending state
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    
    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    
    // Clean up shader modules
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
}

void SimpleRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;
    
    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void SimpleRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffers[i], m_uniformBuffersMemory[i]);
        
        vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
    }
}

void SimpleRenderer::createLightingBuffers() {
    VkDeviceSize bufferSize = sizeof(LightingUBO);
    
    m_lightingBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_lightingBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_lightingBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_lightingBuffers[i], m_lightingBuffersMemory[i]);
        
        vkMapMemory(m_device, m_lightingBuffersMemory[i], 0, bufferSize, 0, &m_lightingBuffersMapped[i]);
    }
}

void SimpleRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void SimpleRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }
}

void SimpleRenderer::updateUniformBuffer(uint32_t currentImage, Camera* camera) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    static bool debugPrinted = false;
    
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    
    if (camera) {
        ubo.view = camera->getViewMatrix();
        ubo.proj = camera->getProjectionMatrix();
        ubo.viewInverse = glm::inverse(ubo.view);
        ubo.projInverse = glm::inverse(ubo.proj);
        ubo.cameraPos = camera->getPosition();
        
        // Debug output for first frame
        if (!debugPrinted) {
            std::cout << "Camera position: (" << ubo.cameraPos.x << ", " << ubo.cameraPos.y << ", " << ubo.cameraPos.z << ")" << std::endl;
            debugPrinted = true;
        }
    } else {
        ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), (float)m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1; // Flip Y for Vulkan
        ubo.cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    }
    
    ubo.time = time;
    
    memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void SimpleRenderer::updateLightingBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    
    LightingUBO lighting{};
    
    // Set up cyberpunk lighting
    lighting.lightCount = 4;
    lighting.ambientLight = glm::vec3(0.02f, 0.02f, 0.05f);
    lighting.exposure = 1.5f;
    
    // Main city light - bright magenta
    lighting.lightPositions[0] = glm::vec3(0.0f, 50.0f, -30.0f);
    lighting.lightColors[0] = glm::vec3(1.0f, 0.3f, 0.8f);
    lighting.lightIntensities[0] = 2.0f;
    
    // Secondary light - cyan
    lighting.lightPositions[1] = glm::vec3(-20.0f, 30.0f, -20.0f);
    lighting.lightColors[1] = glm::vec3(0.2f, 0.8f, 1.0f);
    lighting.lightIntensities[1] = 1.5f;
    
    // Ground light - warm orange
    lighting.lightPositions[2] = glm::vec3(10.0f, 5.0f, -25.0f);
    lighting.lightColors[2] = glm::vec3(1.0f, 0.6f, 0.2f);
    lighting.lightIntensities[2] = 1.0f;
    
    // Animated light - pulsing purple
    lighting.lightPositions[3] = glm::vec3(15.0f, 40.0f, -35.0f + sin(time * 0.5f) * 10.0f);
    lighting.lightColors[3] = glm::vec3(0.8f, 0.2f, 1.0f);
    lighting.lightIntensities[3] = 1.2f + sin(time * 2.0f) * 0.3f;
    
    memcpy(m_lightingBuffersMapped[currentImage], &lighting, sizeof(lighting));
}

void SimpleRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags allocateFlags) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);
    
    VkMemoryAllocateFlagsInfo allocateFlagsInfo{};
    allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocateFlagsInfo.flags = allocateFlags;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    allocInfo.pNext = allocateFlags ? &allocateFlagsInfo : nullptr;
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    
    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

void SimpleRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer SimpleRenderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    return commandBuffer;
}

void SimpleRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

uint32_t SimpleRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("failed to find suitable memory type!");
}

void SimpleRenderer::loadRayTracingFunctions() {
    m_vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureKHR"));
    m_vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureKHR"));
    m_vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureDeviceAddressKHR"));
    m_vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructuresKHR"));
    m_vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureBuildSizesKHR"));
    m_vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesKHR"));
    m_vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_device, "vkGetRayTracingShaderGroupHandlesKHR"));
    m_vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_device, "vkCmdTraceRaysKHR"));

    if (!m_vkCreateAccelerationStructureKHR ||
        !m_vkDestroyAccelerationStructureKHR ||
        !m_vkGetAccelerationStructureDeviceAddressKHR ||
        !m_vkCmdBuildAccelerationStructuresKHR ||
        !m_vkGetAccelerationStructureBuildSizesKHR ||
        !m_vkCreateRayTracingPipelinesKHR ||
        !m_vkGetRayTracingShaderGroupHandlesKHR ||
        !m_vkCmdTraceRaysKHR) {
        throw std::runtime_error("Required ray tracing functions are not available on this device");
    }
}

void SimpleRenderer::createAccelerationStructures() {
    cleanupAccelerationStructures();
    m_rtReady = false;

    if (m_vertexCount == 0 || m_indexCount == 0) {
        std::cout << "[RT] No geometry available for acceleration structures" << std::endl;
        cleanupRayTracingPipeline();
        cleanupRayTracingStorageImage();
        return;
    }

    std::cout << "[RT] Building acceleration structures with " << m_vertexCount << " vertices and " << m_indexCount << " indices" << std::endl;

    VkDeviceAddress vertexAddress = getBufferDeviceAddress(m_vertexBuffer);
    VkDeviceAddress indexAddress = getBufferDeviceAddress(m_indexBuffer);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(GLTFVertex);
    triangles.maxVertex = m_vertexCount;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = m_indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    m_vkGetAccelerationStructureBuildSizesKHR(m_device,
                                              VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                              &buildInfo,
                                              &primitiveCount,
                                              &sizeInfo);

    createBuffer(sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_bottomLevelAS.buffer,
                 m_bottomLevelAS.memory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    VkAccelerationStructureCreateInfoKHR accelCreateInfo{};
    accelCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelCreateInfo.buffer = m_bottomLevelAS.buffer;
    accelCreateInfo.size = sizeInfo.accelerationStructureSize;
    accelCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    if (m_vkCreateAccelerationStructureKHR(m_device, &accelCreateInfo, nullptr, &m_bottomLevelAS.handle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create bottom-level acceleration structure");
    }

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 scratchBuffer,
                 scratchMemory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    VkAccelerationStructureBuildGeometryInfoKHR buildGeomInfo = buildInfo;
    buildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeomInfo.dstAccelerationStructure = m_bottomLevelAS.handle;
    buildGeomInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    m_vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeomInfo, &pRangeInfo);
    endSingleTimeCommands(commandBuffer);

    vkDestroyBuffer(m_device, scratchBuffer, nullptr);
    vkFreeMemory(m_device, scratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = m_bottomLevelAS.handle;
    m_bottomLevelAS.deviceAddress = m_vkGetAccelerationStructureDeviceAddressKHR(m_device, &addressInfo);

    struct InstanceData {
        VkTransformMatrixKHR transform;
        uint32_t instanceCustomIndex : 24;
        uint32_t mask : 8;
        uint32_t instanceShaderBindingTableRecordOffset : 24;
        uint32_t flags : 8;
        uint64_t accelerationStructureReference;
    };

    InstanceData instance{};
    instance.transform = makeIdentityTransformMatrix();
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = m_bottomLevelAS.deviceAddress;

    VkDeviceSize instanceBufferSize = sizeof(InstanceData);
    createBuffer(instanceBufferSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_instancesBuffer,
                 m_instancesMemory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    void* mapped = nullptr;
    vkMapMemory(m_device, m_instancesMemory, 0, instanceBufferSize, 0, &mapped);
    std::memcpy(mapped, &instance, sizeof(InstanceData));
    vkUnmapMemory(m_device, m_instancesMemory);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress = getBufferDeviceAddress(m_instancesBuffer);

    VkAccelerationStructureGeometryKHR topGeometry{};
    topGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    topGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR topBuildInfo{};
    topBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    topBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    topBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    topBuildInfo.geometryCount = 1;
    topBuildInfo.pGeometries = &topGeometry;

    uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR topSizeInfo{};
    topSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    m_vkGetAccelerationStructureBuildSizesKHR(m_device,
                                              VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                              &topBuildInfo,
                                              &instanceCount,
                                              &topSizeInfo);

    createBuffer(topSizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_topLevelAS.buffer,
                 m_topLevelAS.memory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    VkAccelerationStructureCreateInfoKHR topCreateInfo{};
    topCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    topCreateInfo.buffer = m_topLevelAS.buffer;
    topCreateInfo.size = topSizeInfo.accelerationStructureSize;
    topCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    if (m_vkCreateAccelerationStructureKHR(m_device, &topCreateInfo, nullptr, &m_topLevelAS.handle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create top-level acceleration structure");
    }

    VkBuffer topScratchBuffer;
    VkDeviceMemory topScratchMemory;
    createBuffer(topSizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 topScratchBuffer,
                 topScratchMemory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    VkAccelerationStructureBuildGeometryInfoKHR topBuildGeomInfo = topBuildInfo;
    topBuildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    topBuildGeomInfo.dstAccelerationStructure = m_topLevelAS.handle;
    topBuildGeomInfo.scratchData.deviceAddress = getBufferDeviceAddress(topScratchBuffer);

    VkAccelerationStructureBuildRangeInfoKHR topRangeInfo{};
    topRangeInfo.primitiveCount = instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pTopRangeInfo = &topRangeInfo;

    VkCommandBuffer topCommandBuffer = beginSingleTimeCommands();
    m_vkCmdBuildAccelerationStructuresKHR(topCommandBuffer, 1, &topBuildGeomInfo, &pTopRangeInfo);
    endSingleTimeCommands(topCommandBuffer);

    vkDestroyBuffer(m_device, topScratchBuffer, nullptr);
    vkFreeMemory(m_device, topScratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR topAddressInfo{};
    topAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    topAddressInfo.accelerationStructure = m_topLevelAS.handle;
    m_topLevelAS.deviceAddress = m_vkGetAccelerationStructureDeviceAddressKHR(m_device, &topAddressInfo);

    std::cout << "[RT] Acceleration structures built, creating pipeline" << std::endl;

    createRayTracingPipeline();
}

void SimpleRenderer::createRayTracingStorageImage() {
    cleanupRayTracingStorageImage();

    m_rtStorageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {m_swapChainExtent.width, m_swapChainExtent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_rtStorageFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_rtStorageImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing storage image");
    }

    std::cout << "[RT][Debug] Storage image created: "
              << imageInfo.extent.width << "x" << imageInfo.extent.height
              << " format " << imageInfo.format << std::endl;

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_rtStorageImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_rtStorageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate ray tracing storage memory");
    }

    vkBindImageMemory(m_device, m_rtStorageImage, m_rtStorageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_rtStorageImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_rtStorageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_rtStorageImageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing storage image view");
    }

    VkCommandBuffer cmd = beginSingleTimeCommands();
    std::cout << "[RT][Debug] Transition storage image to GENERAL layout" << std::endl;
    transitionImageLayout(m_rtStorageImage,
                          m_rtStorageFormat,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                          0,
                          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

    VkClearColorValue clearColor{0.0f, 0.0f, 1.0f, 1.0f};
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    vkCmdClearColorImage(cmd, m_rtStorageImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);
    std::cout << "[RT][Debug] Cleared storage image to blue" << std::endl;

    endSingleTimeCommands(cmd);
}

void SimpleRenderer::cleanupRayTracingStorageImage() {
    if (m_rtStorageImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_rtStorageImageView, nullptr);
        m_rtStorageImageView = VK_NULL_HANDLE;
    }
    if (m_rtStorageImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_rtStorageImage, nullptr);
        m_rtStorageImage = VK_NULL_HANDLE;
    }
    if (m_rtStorageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_rtStorageMemory, nullptr);
        m_rtStorageMemory = VK_NULL_HANDLE;
    }
}

void SimpleRenderer::cleanupAccelerationStructures() {
    if (m_instancesBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_instancesBuffer, nullptr);
        m_instancesBuffer = VK_NULL_HANDLE;
    }
    if (m_instancesMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_instancesMemory, nullptr);
        m_instancesMemory = VK_NULL_HANDLE;
    }

    auto destroyAS = [&](AccelerationStructure& as) {
        if (as.handle != VK_NULL_HANDLE) {
            m_vkDestroyAccelerationStructureKHR(m_device, as.handle, nullptr);
            as.handle = VK_NULL_HANDLE;
        }
        if (as.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, as.buffer, nullptr);
            as.buffer = VK_NULL_HANDLE;
        }
        if (as.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, as.memory, nullptr);
            as.memory = VK_NULL_HANDLE;
        }
        as.deviceAddress = 0;
    };

    destroyAS(m_bottomLevelAS);
    destroyAS(m_topLevelAS);
}

void SimpleRenderer::createRayTracingPipeline() {
    cleanupRayTracingPipeline();
    createRayTracingStorageImage();

    std::cout << "[RT] Creating ray tracing pipeline" << std::endl;

    std::vector<char> rayGenCode = ShaderManager::readFile("shaders/ray_gen.rgen.spv");
    std::vector<char> missCode = ShaderManager::readFile("shaders/miss.rmiss.spv");
    std::vector<char> chitCode = ShaderManager::readFile("shaders/closest_hit.rchit.spv");

    VkShaderModule rayGenModule = ShaderManager::createShaderModule(m_device, rayGenCode);
    VkShaderModule missModule = ShaderManager::createShaderModule(m_device, missCode);
    VkShaderModule chitModule = ShaderManager::createShaderModule(m_device, chitCode);

    VkPipelineShaderStageCreateInfo raygenStage{};
    raygenStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    raygenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    raygenStage.module = rayGenModule;
    raygenStage.pName = "main";

    VkPipelineShaderStageCreateInfo missStage{};
    missStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStage.module = missModule;
    missStage.pName = "main";

    VkPipelineShaderStageCreateInfo chitStage{};
    chitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    chitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    chitStage.module = chitModule;
    chitStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> stages = {raygenStage, missStage, chitStage};

    VkRayTracingShaderGroupCreateInfoKHR raygenGroup{};
    raygenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenGroup.generalShader = 0;
    raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 1;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = 2;
    hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups = {raygenGroup, missGroup, hitGroup};

    std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo rtLayoutInfo{};
    rtLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    rtLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    rtLayoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device, &rtLayoutInfo, nullptr, &m_rtDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing descriptor set layout");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_rtDescriptorSetLayout;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_rtPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = m_rtPipelineLayout;

    if (m_vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_rtPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing pipeline");
    }

    std::cout << "[RT] Ray tracing pipeline created" << std::endl;

    VkPhysicalDeviceProperties2 properties{};
    m_rtProperties = {};
    m_rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &m_rtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties);

    VkDeviceSize handleSize = m_rtProperties.shaderGroupHandleSize;
    VkDeviceSize handleSizeAligned = (handleSize + m_rtProperties.shaderGroupHandleAlignment - 1) & ~(m_rtProperties.shaderGroupHandleAlignment - 1);
    VkDeviceSize groupCount = static_cast<VkDeviceSize>(groups.size());
    VkDeviceSize sbtSize = groupCount * handleSizeAligned;

    std::vector<uint8_t> handles(sbtSize);
    if (m_vkGetRayTracingShaderGroupHandlesKHR(m_device, m_rtPipeline, 0, static_cast<uint32_t>(groupCount), sbtSize, handles.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to get ray tracing shader group handles");
    }

    createBuffer(sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_rtShaderBindingTable,
                 m_rtShaderBindingTableMemory,
                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(sbtSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingMemory);

    void* data;
    vkMapMemory(m_device, stagingMemory, 0, sbtSize, 0, &data);
    std::memcpy(data, handles.data(), static_cast<size_t>(sbtSize));
    vkUnmapMemory(m_device, stagingMemory);

    copyBuffer(stagingBuffer, m_rtShaderBindingTable, sbtSize);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkDeviceAddress sbtAddress = getBufferDeviceAddress(m_rtShaderBindingTable);
    m_rtRaygenRegion = {sbtAddress, handleSizeAligned, handleSizeAligned};
    m_rtMissRegion = {sbtAddress + handleSizeAligned, handleSizeAligned, handleSizeAligned};
    m_rtHitRegion = {sbtAddress + handleSizeAligned * 2, handleSizeAligned, handleSizeAligned};
    m_rtCallableRegion = {0, 0, 0};

    std::cout << "[RT] Shader binding table setup completed" << std::endl;

    vkDestroyShaderModule(m_device, chitModule, nullptr);
    vkDestroyShaderModule(m_device, missModule, nullptr);
    vkDestroyShaderModule(m_device, rayGenModule, nullptr);

    createRayTracingDescriptorSets();
    m_rtReady = true;
}

void SimpleRenderer::createRayTracingDescriptorSets() {
    std::cout << "[RT] Creating descriptor sets" << std::endl;

    if (m_rtDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_rtDescriptorPool, nullptr);
        m_rtDescriptorPool = VK_NULL_HANDLE;
    }

    VkDescriptorPoolSize poolSizes[4] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT * 2}, // Camera + Lighting
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2}  // Vertex + Index
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 4;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_rtDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_rtDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_rtDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    m_rtDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device, &allocInfo, m_rtDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate ray tracing descriptor sets");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = m_rtStorageImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet imageWrite{};
        imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageWrite.dstSet = m_rtDescriptorSets[i];
        imageWrite.dstBinding = 0;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = &imageInfo;

        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &m_topLevelAS.handle;

        VkWriteDescriptorSet asWrite{};
        asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        asWrite.dstSet = m_rtDescriptorSets[i];
        asWrite.dstBinding = 1;
        asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        asWrite.descriptorCount = 1;
        asWrite.pNext = &asInfo;

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = m_uniformBuffers[i];
        uboInfo.range = sizeof(UniformBufferObject);

    VkDescriptorBufferInfo uboInfoPerFrame{};
    uboInfoPerFrame.buffer = m_uniformBuffers[i];
    uboInfoPerFrame.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet uboWrite{};
    uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uboWrite.dstSet = m_rtDescriptorSets[i];
    uboWrite.dstBinding = 2;
    uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboWrite.descriptorCount = 1;
    uboWrite.pBufferInfo = &uboInfoPerFrame;

    VkDescriptorBufferInfo lightingInfo{};
    lightingInfo.buffer = m_lightingBuffers[i];
    lightingInfo.range = sizeof(LightingUBO);

    VkWriteDescriptorSet lightingWrite{};
    lightingWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    lightingWrite.dstSet = m_rtDescriptorSets[i];
    lightingWrite.dstBinding = 3;
    lightingWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingWrite.descriptorCount = 1;
    lightingWrite.pBufferInfo = &lightingInfo;

        VkDescriptorBufferInfo vertexInfo{};
        vertexInfo.buffer = m_vertexBuffer;
        vertexInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet vertexWrite{};
        vertexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vertexWrite.dstSet = m_rtDescriptorSets[i];
        vertexWrite.dstBinding = 4;
        vertexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vertexWrite.descriptorCount = 1;
        vertexWrite.pBufferInfo = &vertexInfo;

        VkDescriptorBufferInfo indexInfo{};
        indexInfo.buffer = m_indexBuffer;
        indexInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet indexWrite{};
        indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        indexWrite.dstSet = m_rtDescriptorSets[i];
        indexWrite.dstBinding = 5;
        indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        indexWrite.descriptorCount = 1;
        indexWrite.pBufferInfo = &indexInfo;

    std::array<VkWriteDescriptorSet, 6> writes = {imageWrite, asWrite, uboWrite, lightingWrite, vertexWrite, indexWrite};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    std::cout << "[RT] Descriptor sets ready" << std::endl;
}

void SimpleRenderer::cleanupRayTracingPipeline() {
    m_rtReady = false;
    if (m_rtShaderBindingTable != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_rtShaderBindingTable, nullptr);
        m_rtShaderBindingTable = VK_NULL_HANDLE;
    }
    if (m_rtShaderBindingTableMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_rtShaderBindingTableMemory, nullptr);
        m_rtShaderBindingTableMemory = VK_NULL_HANDLE;
    }
    m_rtRaygenRegion = {};
    m_rtMissRegion = {};
    m_rtHitRegion = {};
    m_rtCallableRegion = {};

    if (m_rtDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_rtDescriptorPool, nullptr);
        m_rtDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_rtPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_rtPipeline, nullptr);
        m_rtPipeline = VK_NULL_HANDLE;
    }
    if (m_rtPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_rtPipelineLayout, nullptr);
        m_rtPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_rtDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayout, nullptr);
        m_rtDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void SimpleRenderer::transitionImageLayout(VkImage image,
                                           VkFormat format,
                                           VkImageLayout oldLayout,
                                           VkImageLayout newLayout,
                                           VkPipelineStageFlags srcStage,
                                           VkPipelineStageFlags dstStage,
                                           VkAccessFlags srcAccess,
                                           VkAccessFlags dstAccess) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    std::cout << "[RT][Debug] transitionImageLayout image=0x" << std::hex << reinterpret_cast<uint64_t>(image)
              << std::dec << " " << layoutToString(oldLayout) << " -> " << layoutToString(newLayout)
              << " srcStage=" << srcStage << " dstStage=" << dstStage << std::endl;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd,
                         srcStage,
                         dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    endSingleTimeCommands(cmd);
}
