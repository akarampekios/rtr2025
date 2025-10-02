#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "GLTFLoader.h"

#include "rhi/window.hpp"
#include "rhi/instance.hpp"
#include "rhi/physical_device.hpp"
#include "rhi/logical_device.hpp"

class Camera;
class Scene;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::vec3 cameraPos;
    float time;
};

struct LightingUBO {
    glm::vec3 lightPositions[4];
    glm::vec3 lightColors[4];
    float lightIntensities[4];
    int lightCount;
    glm::vec3 ambientLight;
    float exposure;
};

struct AccelerationStructure {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
};

class SimpleRenderer {
public:
    SimpleRenderer(RHI::Window* window);
    ~SimpleRenderer();

    void beginFrame();
    void render(Camera* camera, Scene* scene);
    void endFrame();
    void initGeometry(Scene* scene);
    
    RHI::LogicalDevice& getDevice() const { return *logicalDevice; }

private:
    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createVertexBuffer(const std::vector<GLTFVertex>& vertices);
    void createIndexBuffer(const std::vector<uint32_t>& indices);
    void createVertexBufferFromData(const std::vector<GLTFVertex>& vertices);
    void createIndexBufferFromData(const std::vector<uint32_t>& indices);
    void createGraphicsPipeline();
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createLightingBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage, Camera* camera);
    void updateLightingBuffer(uint32_t currentImage);
    
    bool findQueueFamilies(VkPhysicalDevice device, uint32_t& graphicsFamily, uint32_t& presentFamily);
    void loadRayTracingFunctions();
    void createAccelerationStructures();
    void cleanupAccelerationStructures();
    void createRayTracingPipeline();
    void createRayTracingDescriptorSets();
    void createRayTracingStorageImage();
    void cleanupRayTracingPipeline();
    void cleanupRayTracingStorageImage();
    
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags allocateFlags = 0);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
    
    VkInstance m_instance;
    VkSurfaceKHR m_surface;
    std::unique_ptr<RHI::Instance> instance = nullptr;
    std::unique_ptr<RHI::PhysicalDevice> physicalDevice = nullptr;
    std::unique_ptr<RHI::LogicalDevice> logicalDevice = nullptr;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    
    VkSwapchainKHR m_swapChain;
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    std::vector<VkImageView> m_swapChainImageViews;
    
    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexBufferMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexBufferMemory;
    uint32_t m_vertexCount;
    uint32_t m_indexCount;
    
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
    
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;
    
    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;
    
    std::vector<VkBuffer> m_lightingBuffers;
    std::vector<VkDeviceMemory> m_lightingBuffersMemory;
    std::vector<void*> m_lightingBuffersMapped;
    
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    
    uint32_t m_currentFrame;
    uint32_t m_imageIndex;
    
    RHI::Window* m_window = nullptr;
    
    uint32_t m_graphicsQueueFamilyIndex;
    uint32_t m_presentQueueFamilyIndex;
    
    AccelerationStructure m_bottomLevelAS;
    AccelerationStructure m_topLevelAS;
    VkBuffer m_instancesBuffer;
    VkDeviceMemory m_instancesMemory;
    
    VkImage m_rtStorageImage;
    VkDeviceMemory m_rtStorageMemory;
    VkImageView m_rtStorageImageView;
    VkFormat m_rtStorageFormat;
    VkDescriptorSetLayout m_rtDescriptorSetLayout;
    VkPipelineLayout m_rtPipelineLayout;
    VkPipeline m_rtPipeline;
    VkDescriptorPool m_rtDescriptorPool;
    std::vector<VkDescriptorSet> m_rtDescriptorSets;
    VkBuffer m_rtShaderBindingTable;
    VkDeviceMemory m_rtShaderBindingTableMemory;
    VkStridedDeviceAddressRegionKHR m_rtRaygenRegion{};
    VkStridedDeviceAddressRegionKHR m_rtMissRegion{};
    VkStridedDeviceAddressRegionKHR m_rtHitRegion{};
    VkStridedDeviceAddressRegionKHR m_rtCallableRegion{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{};

    bool m_rtReady;

    PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR m_vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR m_vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR m_vkCmdTraceRaysKHR = nullptr;
    
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};
