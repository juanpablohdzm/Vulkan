#pragma once
#define GLFW_INCLUDE_VULKAN
#include <optional>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>


#include "Mesh.h"
#include "Utilities.h"


struct QueueFamilyIndices;

class VulkanRenderer
{
    
public:
    VulkanRenderer();

    int32_t Init(GLFWwindow * newWindow);
    void Draw();


    ~VulkanRenderer();
private:
    GLFWwindow* window;

    size_t currentFrame = 0;

    //Scene objects
    std::vector<Mesh> meshList;

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

    //- Synchronisation
    std::vector<VkSemaphore> imageAvailable;
    std::vector<VkSemaphore> renderFinished;
    std::vector<VkFence> drawFences;
    
    

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
    void CreateSynchronisation();

    //- Record functions
    void RecordCommands();
    
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
    void Cleanup();



    
};
