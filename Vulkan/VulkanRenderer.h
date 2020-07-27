#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include "Utilities.h"


struct QueueFamilyIndices;

class VulkanRenderer
{
    
public:
    VulkanRenderer();

    int32_t Init(GLFWwindow * newWindow);


    ~VulkanRenderer();
private:
    GLFWwindow* window;

    //Vulkan components
    VkInstance instance;
    struct 
    {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;
    VkQueue graphicsQueue;
    VkQueue presentationQueue;
    VkSurfaceKHR surface;
    

    //Vulkan functions
    //- Create functions
    void CreateInstance();
    void CreateLogicalDevice();
    void CreateSurface();

    
    //- Get Functions
    void GetPhysicalDevice();

    //-Support functions
    // -- Checker Functions
    bool CheckInstanceExtensionSupport(const std::vector<const char*>& checkExtensions)const;
    bool CheckDeviceExtensionSupport(const VkPhysicalDevice& device) const;
    bool CheckDeviceSuitable(const VkPhysicalDevice& device) const;
    bool CheckValidationLayerSupport() const;
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    //-- Getter Functions
    QueueFamilyIndices& GetQueueFamilies(const VkPhysicalDevice& device) const;
    SwapChainDetails GetSwapChainDetails(const VkPhysicalDevice& device) const;

    //- Destroy functions
    void Cleanup() const;



    
};
