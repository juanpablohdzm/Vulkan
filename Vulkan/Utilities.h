#pragma once
#include <iostream>
#include <vector>
#include <fstream>

const std::vector<const char*> deviceExtensions ={
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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