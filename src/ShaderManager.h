#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <vulkan/vulkan.h>

class ShaderManager {
public:
    static std::vector<char> readFile(const std::string& filename);
    static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    static VkShaderModule loadShader(VkDevice device, const std::string& filename);
    
private:
    static std::filesystem::path getShaderPath(const std::string& filename);
};
