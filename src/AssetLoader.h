#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

struct MeshData {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    glm::mat4 transform;
    std::string materialName;
};

class AssetLoader {
public:
    static bool loadGLTF(const std::string& filepath, std::vector<MeshData>& meshes);
    static bool loadOBJ(const std::string& filepath, std::vector<MeshData>& meshes);
    
private:
    static void processNode(const void* node, const void* scene, std::vector<MeshData>& meshes);
    static MeshData processMesh(const void* mesh, const void* scene);
};

