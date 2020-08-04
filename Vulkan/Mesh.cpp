#include "Mesh.h"

Mesh::Mesh(): vertexCount(0), vertexBuffer(nullptr),
    vertexBufferMemory(nullptr), physicalDevice(nullptr),
    device(nullptr)
{
}

Mesh::Mesh(const VkPhysicalDevice& newPhysicalDevice, const VkDevice& newDevice,VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<Vertex>* vertices):
    vertexCount(vertices->size()),
    physicalDevice(newPhysicalDevice),
    device(newDevice)
{
    CreateVertexBuffer(transferQueue,transferCommandPool,vertices);
}


void Mesh::CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,const std::vector<Vertex>* vertices)
{
    VkDeviceSize bufferSize = sizeof(Vertex)* vertices->size();

    //Temporary buffer to "stage" vertex data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    //Create staging buffer and allocate memory to it
    CreateBuffer(physicalDevice,device,bufferSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer,&stagingBufferMemory);

    //map memory to vertex buffer
    void * data;
    vkMapMemory(device,stagingBufferMemory,0,bufferSize,0,&data);
    memcpy(data,vertices->data(),static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    //Create buffer with Transfer dst bit to mark as recipient of transfer data (also vertex buffer)
    CreateBuffer(physicalDevice, device,bufferSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&vertexBuffer,&vertexBufferMemory);

    CopyBuffer(device,transferQueue,transferCommandPool,stagingBuffer,vertexBuffer,bufferSize);

    vkFreeMemory(device,stagingBufferMemory,nullptr);
    vkDestroyBuffer(device,stagingBuffer,nullptr);
    
}

void Mesh::DestroyVertexBuffer()
{
    vkFreeMemory(device,vertexBufferMemory,nullptr);
    vkDestroyBuffer(device,vertexBuffer,nullptr);
}

Mesh::~Mesh()
{
}
