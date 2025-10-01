#include "ShaderManager.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

std::vector<char> ShaderManager::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkShaderModule ShaderManager::createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

VkShaderModule ShaderManager::loadShader(VkDevice device, const std::string& filename) {
    std::filesystem::path shaderPath = getShaderPath(filename);
    std::cout << "[ShaderManager] Loading shader from: " << shaderPath.string() << std::endl;
    auto code = readFile(shaderPath.string());
    return createShaderModule(device, code);
}

std::filesystem::path ShaderManager::getShaderPath(const std::string& filename) {
    const std::vector<std::filesystem::path> searchDirectories = {
        std::filesystem::path("./build/bin/Release/shaders"),
        std::filesystem::path("../bin/Release/shaders"),
        std::filesystem::path("./build/bin/Debug/shaders"),
        std::filesystem::path("../bin/Debug/shaders"),
        std::filesystem::path("./build/bin/shaders"),
        std::filesystem::path("../bin/shaders"),
        std::filesystem::path("./shaders"),
        std::filesystem::path("../shaders"),
        std::filesystem::path("../../shaders"),
        std::filesystem::path("../../bin/shaders")
    };

    for (const auto& dir : searchDirectories) {
        std::error_code ec;
        auto candidate = dir / filename;
        if (std::filesystem::exists(candidate, ec)) {
            auto resolved = std::filesystem::weakly_canonical(candidate, ec);
            if (!ec) {
                return resolved;
            }
            return candidate;
        }
    }

    throw std::runtime_error("Shader not found: " + filename);
}
