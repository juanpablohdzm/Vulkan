#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utilities.h"

struct Model
{
    glm::mat4 currentModel;
};

class Mesh
{
public:
    Mesh();
    Mesh(const VkPhysicalDevice& newPhysicalDevice,const VkDevice& newDevice,VkQueue transferQueue,
        VkCommandPool transferCommandPool,const std::vector<Vertex>* vertices, const std::vector<uint32_t>* indices);

    void SetModel(glm::mat4 _model) {model.currentModel = _model;}
    glm::mat4 GetModel() const { return model.currentModel;} 
    
    void DestroyBuffers();
    
    int GetVertexCount() const{return vertexCount;}
    VkBuffer GetVertexBuffer() const{return vertexBuffer;}

    int GetIndicesCount() const{return indexCount;}
    VkBuffer GetIndexBuffer() const{return indexBuffer;}

    ~Mesh();


private:
    Model model;
    
    int vertexCount;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    size_t indexCount;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    void CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,const std::vector<Vertex>* vertices);
    void CreateIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,const std::vector<uint32_t>* indices);
};
