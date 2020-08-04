#include "Mesh.h"

Mesh::Mesh(): vertexCount(0), vertexBuffer(nullptr),
    vertexBufferMemory(nullptr), physicalDevice(nullptr),
    device(nullptr)
{
}

Mesh::Mesh(const VkPhysicalDevice& newPhysicalDevice, const VkDevice& newDevice, const std::vector<Vertex>* vertices):
    vertexCount(vertices->size()),
    physicalDevice(newPhysicalDevice),
    device(newDevice)
{
    CreateVertexBuffer(vertices);
}


void Mesh::CreateVertexBuffer(const std::vector<Vertex>* vertices)
{
    
    //Information to create a buffer (doesn't include assigning memory)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(Vertex)*vertexCount;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; //Multiple types of buffer possible, we want Vertex Buffer
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult  result = vkCreateBuffer(device,&bufferInfo,nullptr,&vertexBuffer);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Vertex Buffer");

    //Get buffer memory requirments
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device,vertexBuffer,&memoryRequirements);

    //Allocate memory to buffer
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    //Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(device,&memoryAllocateInfo,nullptr,&vertexBufferMemory);
    if(result !=VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }

    //Allocate memory to given vertex buffer
    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory,0);

    //map memory to vertex buffer
    void * data;
    vkMapMemory(device,vertexBufferMemory,0,bufferInfo.size,0,&data);
    memcpy(data,vertices->data(),static_cast<size_t>(bufferInfo.size));
    vkUnmapMemory(device, vertexBufferMemory);
}

uint32_t Mesh::FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    //Get properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice,&memoryProperties);

    for (uint32_t i=0; i<memoryProperties.memoryTypeCount; i++)
    {
        if((allowedTypes & (1<<i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties)== properties)
        {
            //This memory type is valid, return it's index
            return i;
        }
    }
    return std::numeric_limits<uint32_t>::max();
}

void Mesh::DestroyVertexBuffer()
{
    vkFreeMemory(device,vertexBufferMemory,nullptr);
    vkDestroyBuffer(device,vertexBuffer,nullptr);
}

Mesh::~Mesh()
{
}
