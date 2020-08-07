﻿#pragma once
#define GLFW_INCLUDE_VULKAN
#include <optional>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>


#include "Mesh.h"
#include "stb_image.h"
#include "Utilities.h"



struct QueueFamilyIndices;

class VulkanRenderer
{
    
public:
    VulkanRenderer();

    int32_t Init(GLFWwindow * newWindow);
    void UpdateModel(int modelId,glm::mat4 newModel);
    void Draw();


    ~VulkanRenderer();
private:
    GLFWwindow* window;

    size_t currentFrame = 0;

    //Scene objects
    std::vector<Mesh> meshList;

    //Scene settings
    struct UboViewProjection
    {
        glm::mat4 projection;
        glm::mat4 view;
    } uboViewProjection;

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

    VkImage depthBufferImage;
    VkDeviceMemory depthBufferImageMemory;
    VkImageView depthBufferImageView;
    
    //-Descriptors
    VkDescriptorSetLayout descriptorSetLayout;
    VkPushConstantRange pushConstantRange;
    
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkBuffer> vpUniformBuffer;
    std::vector<VkDeviceMemory> vpUniformBufferMemory;

    //std::vector<VkBuffer> modelUniformBuffer;
    //std::vector<VkDeviceMemory> modelUniformBufferMemory;
    
    //-Assets
    std::vector<VkImage> textureImages;
    std::vector<VkDeviceMemory> textureImageMemory;
    
    //- Pipeline
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;

    // -Pools
    VkCommandPool graphicsCommandPool;
    
    // - Utility
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    //VkDeviceSize minUniformBufferOffset;
    //size_t modelUniformAlignment;
    //Model* modelTransferSpace;

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
    void CreateDescriptorSetLayout();
    void CreatePushConstantRange();
    void CreateGraphicsPipeline();
    void CreateDepthBufferImage();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSynchronisation();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();

    void UpdateUniformBuffer(uint32_t imageIndex);
    //- Record functions
    void RecordCommands(uint32_t currentImage);
    
    //- Get Functions
    void GetPhysicalDevice();

    // - Allocate Functions
    //void AllocateDynamicBufferTransferSpace();

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
    VkFormat ChooseSupportedFormat(const std::vector<VkFormat> &formats,VkImageTiling tiling, VkFormatFeatureFlags featureFlags)const;
    
    //--Create functions
    VkImage CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory);
    VkImageView CreateImageView(VkImage image, VkFormat format,VkImageAspectFlags aspectFlags) const;
    VkShaderModule CreateShaderModule(const std::vector<char>& code);

    int CreateTexture(const std::string& fileName);
    
    // - Loader functions
    stbi_uc*  LoadTextureFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize) const;
    
    //- Destroy functions
    void Cleanup();



    
};
