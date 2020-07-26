#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

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


    //Vulkan functions
    //- Create functions
    void CreateInstance();
    void CreateLogicalDevice();

    
    //- Get Functions
    void GetPhysicalDevice();

    //-Support functions
    // -- Checker Functions
    bool CheckInstanceExtensionSupport(const std::vector<const char*>& checkExtensions)const;
    bool CheckDeviceSuitable(const VkPhysicalDevice& device) const;

    //-- Getter Functions
    QueueFamilyIndices& GetQueueFamilies(const VkPhysicalDevice& device) const;

    //- Destroy functions
    void Cleanup() const;


    //Vulkan components
    struct 
    {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;
    VkQueue graphicsQueue;
    
};
