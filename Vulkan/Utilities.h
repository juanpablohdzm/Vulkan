#pragma once
#include <iostream>
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
const int MAX_FRAME_DRAWS = 2;

const std::vector<const char*> deviceExtensions ={
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex
{
    glm::vec3 pos; //Vertex position
    glm::vec3 col;
};

//Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices
{
    int graphicsFamily = -1; // Location of Graphics Queue Family
    int presentationFamily = -1; //Location of Presentation Queue Family
    bool IsValid() const {return graphicsFamily >= 0 && presentationFamily >=0;}
};

const std::vector<const char*> validationLayers ={
    "VK_LAYER_KHRONOS_validation"
};

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities; //Surface properties e.g. image size
    std::vector<VkSurfaceFormatKHR> formats; //Surface image formats e.g. RGBA and size of each color
    std::vector<VkPresentModeKHR> presentationModes; //How images should be presented to screen
};

struct SwapchainImage
{
    VkImage image;
    VkImageView imageView;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

static std::vector<char> ReadFile(const std::string& filename)
{
    //Open stream from given file
    // std::ios::binary tells stream to read file as binary
    // std::ios::ate tells stream to start reading from end of file
    std::ifstream file(filename,std::ios::binary | std::ios::ate); //ate = at end

    if(!file.is_open())
        throw std::runtime_error("Failed to open a file!");

    //Get current read position and use to resize file buffer
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> fileBuffer(fileSize);

    //Move read position to start of the file
    file.seekg(0);

    //Read the file data into the buffer
    file.read(fileBuffer.data(),fileSize);

    file.close();

    return fileBuffer;
}

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice,uint32_t allowedTypes, VkMemoryPropertyFlags properties)
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

static void CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize,
    VkBufferUsageFlags bufferUsage,  VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
    //Information to create a buffer (doesn't include assigning memory)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = bufferUsage; //Multiple types of buffer possible
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult  result = vkCreateBuffer(device,&bufferInfo,nullptr,buffer);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Vertex Buffer");

    //Get buffer memory requirments
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device,*buffer,&memoryRequirements);

    //Allocate memory to buffer
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice,memoryRequirements.memoryTypeBits,
        bufferProperties);
    //Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(device,&memoryAllocateInfo,nullptr,bufferMemory);
    if(result !=VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }

    //Allocate memory to given vertex buffer
    vkBindBufferMemory(device, *buffer, *bufferMemory,0);

}

static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer,
    VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    //Command buffer to hold tranfer commands
    VkCommandBuffer transferCommandBuffer{};
    //Command buffer details
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool;
    allocInfo.commandBufferCount = 1;

    //Allocate command buffer from pool
    vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);

    //Infomation to begin the command buffer record
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    //begin recording transfer commands
    vkBeginCommandBuffer(transferCommandBuffer,&beginInfo);

    //Region of data to copy from an to
    VkBufferCopy bufferCopyRegion{};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    //Command to copy src buffer to dst buffer
    vkCmdCopyBuffer(transferCommandBuffer,srcBuffer,dstBuffer,1,&bufferCopyRegion);

    //End commands
    vkEndCommandBuffer(transferCommandBuffer);

    //Queue submission information
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCommandBuffer;

    //Submit transfer command to transfer queue and wait unitl it finishes
    vkQueueSubmit(transferQueue,1,&submitInfo,VK_NULL_HANDLE);
    vkQueueWaitIdle(transferQueue);

    //Free temporary command buffer back to pool
    vkFreeCommandBuffers(device, transferCommandPool,1,&transferCommandBuffer);
}