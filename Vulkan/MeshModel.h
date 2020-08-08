#pragma once
#include <string>
#include <vector>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

class Mesh;

class MeshModel
{
public:

    MeshModel();
    MeshModel(std::vector<Mesh> newMeshList);

    size_t GetMeshCount()const {return meshList.size();}

    void SetModel(glm::mat4 newModel) {model = newModel;}
    glm::mat4 GetModel() const {return model;}

    Mesh* GetMesh(size_t index);

    static std::vector<std::string> LoadMaterials(const aiScene* scene);
    static std::vector<Mesh> LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
        aiNode* node, const aiScene* scene,const std::vector<int>& matToTex);
    static Mesh LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
                         aiMesh* mesh, const aiScene* scene, const std::vector<int>& matToTex);
    
    void DestroyMeshModel();
    ~MeshModel();

private:

    std::vector<Mesh> meshList;
    glm::mat4 model;
};
