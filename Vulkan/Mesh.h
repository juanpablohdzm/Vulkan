#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utilities.h"
class Mesh
{
public:
    Mesh();
    Mesh(const VkPhysicalDevice& newPhysicalDevice,const VkDevice& newDevice,VkQueue transferQueue, VkCommandPool transferCommandPool,const std::vector<Vertex>* vertices);

    void DestroyVertexBuffer();
    
    int GetVertexCount() const{return vertexCount;}
    VkBuffer GetVertexBuffer() const{return vertexBuffer;}

    ~Mesh();


private:
    int vertexCount;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    void CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,const std::vector<Vertex>* vertices);
};
