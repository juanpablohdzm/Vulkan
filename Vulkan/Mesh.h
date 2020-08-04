#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utilities.h"
class Mesh
{
public:
    Mesh();
    Mesh(const VkPhysicalDevice& newPhysicalDevice,const VkDevice& newDevice,const std::vector<Vertex>* vertices);

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

    void CreateVertexBuffer(const std::vector<Vertex>* vertices);
    uint32_t FindMemoryTypeIndex(uint32_t allowedTypes,VkMemoryPropertyFlags properties);
};
