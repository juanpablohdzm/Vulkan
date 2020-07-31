#pragma once
#define GLFW_INCLUDE_VULKAN
#include <optional>
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
    // - Main
    VkInstance instance;
    struct 
    {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;
    VkQueue graphicsQueue;
    VkQueue presentationQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchainKhr;
    
    std::vector<SwapchainImage> swapchainImages;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;

    //- Pipeline
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;

    // -Pools
    VkCommandPool graphicsCommandPool;
    
    // - Utility
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    
    

    //Vulkan functions
    //- Create functions
    void CreateInstance();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateSwapChain();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    
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

    //--Choose functions
    std::optional<VkSurfaceFormatKHR> ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes) const;
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const;

    //--Create functions
    VkImageView CreateImageView(VkImage image, VkFormat format,VkImageAspectFlags aspectFlags) const;
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    

    //- Destroy functions
    void Cleanup() const;



    
};
