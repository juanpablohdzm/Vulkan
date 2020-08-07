#include "Mesh.h"

Mesh::Mesh(): model(), vertexCount(0), vertexBuffer(nullptr),
              vertexBufferMemory(nullptr), indexCount(0),
              indexBuffer(nullptr), indexBufferMemory(nullptr),
              physicalDevice(nullptr), device(nullptr)
{
}

Mesh::Mesh(const VkPhysicalDevice& newPhysicalDevice, const VkDevice& newDevice,VkQueue transferQueue,
           VkCommandPool transferCommandPool, const std::vector<Vertex>* vertices, const std::vector<uint32_t>* indices, int newTexID):
    vertexCount(vertices->size()),
    physicalDevice(newPhysicalDevice),
    device(newDevice),
    indexCount(indices->size()),
    texId(newTexID)
{
    CreateVertexBuffer(transferQueue,transferCommandPool,vertices);
    CreateIndexBuffer(transferQueue,transferCommandPool,indices);
    model.currentModel = glm::mat4(1.0f);
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

void Mesh::CreateIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
    const std::vector<uint32_t>* indices)
{
    VkDeviceSize bufferSize = sizeof(uint32_t)* indices->size();
    
    //Temporary buffer to "stage" index data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(physicalDevice,device,bufferSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer,&stagingBufferMemory);

    //map memory to index buffer
    void * data;
    vkMapMemory(device,stagingBufferMemory,0,bufferSize,0,&data);
    memcpy(data,indices->data(),static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    //Create buffer with Transfer dst bit to mark as recipient of transfer data (also vertex buffer)
    CreateBuffer(physicalDevice, device,bufferSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&indexBuffer,&indexBufferMemory);

    CopyBuffer(device,transferQueue,transferCommandPool,stagingBuffer,indexBuffer,bufferSize);

    vkFreeMemory(device,stagingBufferMemory,nullptr);
    vkDestroyBuffer(device,stagingBuffer,nullptr);
}

void Mesh::DestroyBuffers()
{
    vkFreeMemory(device,vertexBufferMemory,nullptr);
    vkDestroyBuffer(device,vertexBuffer,nullptr);

    vkFreeMemory(device,indexBufferMemory,nullptr);
    vkDestroyBuffer(device,indexBuffer,nullptr);
}

Mesh::~Mesh()
{
}
