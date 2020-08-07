﻿#include "VulkanRenderer.h"

#include <algorithm>
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "Utilities.h"
#include "stb_image.h" 
#include <iostream>
#include <optional>
#include <vector>
#include <set>
#include <stdbool.h>


VulkanRenderer::VulkanRenderer():
    window(nullptr), uboViewProjection(), instance(nullptr),
    mainDevice(), graphicsQueue(nullptr),
    presentationQueue(nullptr), surface(nullptr),
    swapchainKhr(nullptr), descriptorSetLayout(nullptr),
    pushConstantRange(),
    descriptorPool(nullptr), graphicsPipeline(nullptr),
    pipelineLayout(nullptr), renderPass(nullptr),
    graphicsCommandPool(nullptr), swapChainImageFormat(),
    swapChainExtent()
{
}

int32_t VulkanRenderer::Init(GLFWwindow* newWindow)
{
    window = newWindow;
    try
    {
        CreateInstance();
        CreateSurface();
        GetPhysicalDevice();
        CreateLogicalDevice();   
        CreateSwapChain();
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreatePushConstantRange();
        CreateGraphicsPipeline();
        CreateDepthBufferImage();
        CreateFramebuffers();
        CreateCommandPool();

        int firstTexture  = CreateTexture("Background.png");
        
        uboViewProjection.projection = glm::perspective(glm::radians(45.0f),(float) swapChainExtent.width/(float)swapChainExtent.height,0.1f,100.0f);
        uboViewProjection.view = glm::lookAt(glm::vec3(0.0f,0.0f,2.0f),glm::vec3(0.0f,0.0f,0.0f),glm::vec3(0.0f,1.0f,0.0f));

        uboViewProjection.projection[1][1] *= -1;
        //Create a mesh
        //Vertex data
        std::vector<Vertex> meshVertices ={
        {{0.4f,-0.4f,0.0f},{1.0f,0.0f,0.0f}},
        {{0.4f,0.4f,0.0f},{1.0f,0.0f,0.0f}},
        {{-0.4f,0.4f,0.0f},{1.0f,0.0f,0.0f}},          
        {{-0.4f,-0.4f,0.0f},{1.0f,0.0f,0.0f}},
        };

        std::vector<Vertex> meshVertices2 ={
        {{0.4f,-0.4f,0.0f},{0.0f,0.0f,1.0f}},
        {{0.4f,0.4f,0.0f},{0.0f,1.0f,0.0f}},
        {{-0.4f,0.4f,0.0f},{1.0f,0.0f,0.0f}},          
        {{-0.4f,-0.4f,0.0f},{0.0f,1.0f,0.0f}},
        };

        //Index data
        std::vector<uint32_t> meshIndices ={
            0,1,2,
            2,3,0
        };
        meshList.push_back(Mesh(mainDevice.physicalDevice,mainDevice.logicalDevice,graphicsQueue,graphicsCommandPool,&meshVertices,&meshIndices));
        meshList.push_back(Mesh(mainDevice.physicalDevice,mainDevice.logicalDevice,graphicsQueue,graphicsCommandPool,&meshVertices2,&meshIndices));

        CreateCommandBuffers();
        //AllocateDynamicBufferTransferSpace();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateSynchronisation();
    }
    catch (const std::runtime_error& e)
    {
        printf("Error : %s  \n",e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void VulkanRenderer::UpdateModel(int modelId,glm::mat4 newModel)
{
    if(modelId >= meshList.size()) return;

    meshList[modelId].SetModel(newModel);
}

void VulkanRenderer::Draw()
{
    //1. Get next available image to draw to and set something to signal when we're finish with the image (a semaphore)
    vkWaitForFences(mainDevice.logicalDevice,1,&drawFences[currentFrame],VK_TRUE,std::numeric_limits<uint64_t>::max());
    vkResetFences(mainDevice.logicalDevice,1,&drawFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(mainDevice.logicalDevice,swapchainKhr,
        std::numeric_limits<uint64_t>::max(),imageAvailable[currentFrame],VK_NULL_HANDLE,&imageIndex);

    RecordCommands(imageIndex);
    UpdateUniformBuffer(imageIndex);


    //2. Submit command buffer to queue for execution, making sure it waits for the image to be signalled as available before drawing
    //and signals when it has finished working
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount =1;
    submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
    VkPipelineStageFlags waitStages[] ={VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages; //Stages to check semaphores at
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex]; //Command buffer to submit
    submitInfo.signalSemaphoreCount = 1; //Number of semaphores to signal
    submitInfo.pSignalSemaphores = &renderFinished[currentFrame]; //Semaphores to signal when command buffer finishes

    VkResult result = vkQueueSubmit(graphicsQueue,1,&submitInfo,drawFences[currentFrame]);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to submit command buffer to queue");
    //3. Present image to screen when it has signalled finished rendering
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainKhr;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentationQueue,&presentInfo);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to present image");

    currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::CreateInstance()
{
    //Check if validation layers are supported by the GPU
    if (!CheckValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    
    //Information about the application itself
    //Most data here doesn't affect the program and is for developer convenience

    VkApplicationInfo appInfo{};// Avoid null exceptions by using {} to initialize
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan APP"; //Custom name of the application
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0); //Custom version of the application
    appInfo.pEngineName = "No Engine"; //Custom engine name
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);//Custom engine version
    appInfo.apiVersion = VK_API_VERSION_1_0; //Vulkan version 

    


    //Set up extension instance will use
    uint32_t glfwExtensionCount = 0; //GLFW may require multiple extension
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // Extensions passed as array of cstrings, so need pointer (the array) to pointer (the cstring)

    //Create lit to hold instance extensions
    //Add GLFW extensions to list of extensions
    std::vector<const char*> instanceExtensions(glfwExtensions,glfwExtensions+glfwExtensionCount);
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    //Check instance extension supported...
    if(!CheckInstanceExtensionSupport(instanceExtensions))
    {
        throw std::runtime_error("VKInstance does not support required extensions!");
    }
    
    //Creation information for a VKInstance
    VkInstanceCreateInfo createInfo{};// Avoid null exceptions by using {} to initialize
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    //Add the validation layers to the instance
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    PopulateDebugMessengerCreateInfo(debugCreateInfo);
    
    createInfo.enabledLayerCount  = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);

    //Create instance;
    const VkResult& result = vkCreateInstance(&createInfo,nullptr,&instance);

    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Vulkan instance");
    }
}

void VulkanRenderer::GetPhysicalDevice()
{
    // Enumarate physical devices the vkInstance can access
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance,&deviceCount,nullptr);

    // If no devices available, then none support vulkan!
    if(!deviceCount)
    {
        throw std::runtime_error("Can't find GPUs that support Vulkan instance");
    }
    
    //Get list of physical devices
    std::vector<VkPhysicalDevice> deviceList(deviceCount);
    vkEnumeratePhysicalDevices(instance,&deviceCount,deviceList.data());

    for (const VkPhysicalDevice& device: deviceList)
    {
        if(CheckDeviceSuitable(device))
        {
            mainDevice.physicalDevice = device;
            break;
        }
    }

    if(!mainDevice.physicalDevice)
        throw std::runtime_error("No physical device found");

    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
    
    //minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

//void VulkanRenderer::AllocateDynamicBufferTransferSpace()
//{
    // //Calculate alignment of model data
    // modelUniformAlignment = (sizeof(Model)+ minUniformBufferOffset - 1) & ~(minUniformBufferOffset-1);
    //
    // //Create space in memory to hold dynamic buffer that is aligned to our required alignment and holds MAX_OBJECTS
    // modelTransferSpace = static_cast<Model*>(_aligned_malloc(modelUniformAlignment*MAX_OBJECTS, modelUniformAlignment));
//}

void VulkanRenderer::CreateLogicalDevice()
{
    //Get the queue family indices for the chosen Physical device
    QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);

    //Vector for queue creation information, and set for family indices
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentationFamily};
    
    //Queues the logical device needs to create and info to do so
    for (int queueFamilyIndex : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex; //The index of the family to create a queue from.
        queueCreateInfo.queueCount = 1; //Number of queues to create
        float priority  = 1.0f;
        queueCreateInfo.pQueuePriorities = &priority; //Vulkan needs to know how to handle multiple queues, so decide priority (1 = highest priority)

        queueCreateInfos.push_back(queueCreateInfo);
    }

    //Physical device features the logical device will be using
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    //Information to create logical device (sometimes called "device")
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()); //Number of queue create infos
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data(); //List of queue create infos so device can create required queues
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); //Number of enabled logical device extensions
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data(); //List of enabled logical device extensions
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures; //Physical device features logical device will use

    //Add validation layers to the logic device
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    PopulateDebugMessengerCreateInfo(debugCreateInfo);

    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    deviceCreateInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);

    //Create the logical device for the given physical device
    VkResult result = vkCreateDevice(mainDevice.physicalDevice,&deviceCreateInfo,nullptr,&mainDevice.logicalDevice);
    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a logical device");
    }

    //Queues are created at the same time as the device...
    //So we want to handle the queues
    //From given logical device, of given Queue family, of given queue index (0 since only one queue), place reference in given VkQueue
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily,0,&graphicsQueue);
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily,0,&presentationQueue);
}

void VulkanRenderer::CreateSurface()
{
    //Create surface (creates a surface create info struct, runs the create surface function, returns result)
    VkResult result = glfwCreateWindowSurface(instance,window,nullptr,&surface);
    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a surface!");
    }
    
}

void VulkanRenderer::CreateSwapChain()
{
    //Get swap chain details so we can pick best settings
    SwapChainDetails swapChainDetails = GetSwapChainDetails(mainDevice.physicalDevice);

    //1.Choose best surface format
    std::optional<VkSurfaceFormatKHR> opt = ChooseBestSurfaceFormat(swapChainDetails.formats);
    VkSurfaceFormatKHR surfaceFormat = opt.has_value()? opt.value() : swapChainDetails.formats[0];
    //2.Choose best presentation mode
    VkPresentModeKHR presentModeKhr = ChooseBestPresentationMode(swapChainDetails.presentationModes);
    //3. Choose swap chain image resolution
    VkExtent2D extent2D = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);

    VkSwapchainCreateInfoKHR swapchainCreateInfoKhr{};
    swapchainCreateInfoKhr.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfoKhr.surface = surface;
    swapchainCreateInfoKhr.imageFormat = surfaceFormat.format;
    swapchainCreateInfoKhr.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfoKhr.presentMode = presentModeKhr;
    swapchainCreateInfoKhr.imageExtent = extent2D;
    
    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
    if(swapChainDetails.surfaceCapabilities.maxImageCount > 0 &&
        swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)         
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    
    swapchainCreateInfoKhr.minImageCount = imageCount;
    swapchainCreateInfoKhr.imageArrayLayers = 1;
    swapchainCreateInfoKhr.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfoKhr.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
    swapchainCreateInfoKhr.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfoKhr.clipped = VK_TRUE;

    //Get Queue Family indices
    QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);

    //If Graphics and presentation families are different, then swapchain must let images be shared between families
    if(indices.graphicsFamily != indices.presentationFamily)
    {
        uint32_t queueFamilyIndices[] ={(uint32_t)indices.graphicsFamily, (uint32_t)indices.presentationFamily};
        
        swapchainCreateInfoKhr.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfoKhr.queueFamilyIndexCount = 2;
        swapchainCreateInfoKhr.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        swapchainCreateInfoKhr.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfoKhr.queueFamilyIndexCount = 0;
        swapchainCreateInfoKhr.pQueueFamilyIndices = nullptr;
    }
    //If old swap chain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
    swapchainCreateInfoKhr.oldSwapchain = VK_NULL_HANDLE;

    //Create swapchain
    VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice,&swapchainCreateInfoKhr,nullptr,&swapchainKhr);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Error creating swapchain");

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent2D;

    //Get swap chain images
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice,swapchainKhr, &swapchainImageCount,nullptr);

    std::vector<VkImage> images(swapchainImageCount);
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice,swapchainKhr, &swapchainImageCount,images.data());

    for (VkImage image : images)
    {
        SwapchainImage swapchainImage{};
        swapchainImage.image = image;
        swapchainImage.imageView = CreateImageView(image,swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        swapchainImages.push_back(swapchainImage);
    }
    
}

void VulkanRenderer::CreateRenderPass()
{
    //Color attachment of render pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;    //Format to use for attachment
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;     //Number of samples to write for multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    //Describes what do with attachment before rendering
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;    //Describes what to do with attachment after rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; //Describes what to do with stencil after rendering

    //Famebuffer data will be stored as an image, but images can be given different data layouts
    //to give optimal use for certain operations
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //Image data layout before render pass starts
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //Image data layout after render pass (to change to)

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = ChooseSupportedFormat(
        {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    
    //Attachment reference uses an attachment index tha refers to index in the attachment list passed to renderPassCreateInfo
    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //Information about a particular subpass the render pass is using
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; //Pipeline type subpass is to be bound to
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    //Need to determine when layout transition occur using subpass dependencies
    std::array<VkSubpassDependency,2> subpassDependencies;

    //Conversion from layout undefined to layout color attachment 
    //Transition must happen after...
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    
    //But must happen before 
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    //Transition must happen after...
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   
    //But must happen before 
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    std::array<VkAttachmentDescription,2> renderPassAttachments = {colorAttachment,depthAttachment};
    //Create info for render pass
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
    renderPassCreateInfo.pAttachments = renderPassAttachments.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(mainDevice.logicalDevice,& renderPassCreateInfo,nullptr,&renderPass);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Error creating render pass");
    
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
    //UboViewProjection binding info
    VkDescriptorSetLayoutBinding vpLayoutBinding{};
    vpLayoutBinding.binding = 0; //Where this data is binded to  in shader
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpLayoutBinding.descriptorCount = 1;
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vpLayoutBinding.pImmutableSamplers = nullptr; //For texture can make sampler data unchangable

    //Model binding info
    // VkDescriptorSetLayoutBinding modelLayoutBinding{};
    // modelLayoutBinding.binding = 1;
    // modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    // modelLayoutBinding.descriptorCount = 1;
    // modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // modelLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings ={vpLayoutBinding};
    
    //Create descriptor set layout with given bindings
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutCreateInfo.pBindings = layoutBindings.data();

    //Create descriptor set layout
    VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice,&layoutCreateInfo,nullptr,&descriptorSetLayout);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a descriptor set layout");
}

void VulkanRenderer::CreatePushConstantRange()
{
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //Shader stage push constant
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model);
}

void VulkanRenderer::CreateGraphicsPipeline()
{
    auto vertexShaderCode = ReadFile("Shaders/vert.spv");   
    auto fragmentShaderCode = ReadFile("Shaders/frag.spv");

    //Build shader modules to link to graphics pipeline
    VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

    // --Shader stage creation information
    // Vertex stage creation information
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo{};
    vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderCreateInfo.module = vertexShaderModule;
    vertexShaderCreateInfo.pName = "main";

    // Fragment stage creation information
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo{};
    fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderCreateInfo.module = fragmentShaderModule;
    fragmentShaderCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

    //How the data for a single vertex(including info such as position, color, texture uv, normals, etc.) is as  a whole
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0; //Can bind multiple streams of data, this defines which one
    bindingDescription.stride = sizeof(Vertex); //Size of a single vertex object
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //How to move between data after each vertex. Move on to the next vertex

    //How the data for an attribute is defined within a vertex
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
    //Position attribute
    attributeDescriptions[0].binding = 0; //Which binding the data is at. Same as above 
    attributeDescriptions[0].location = 0; //Location in shader where data will be read from
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; //Format the data will take (also helps define the size of data)
    attributeDescriptions[0].offset = offsetof(Vertex,pos); //Where this attribute is defined in the data for a single vertex

    //Color attribute
    attributeDescriptions[1].binding = 0; //Which binding the data is at. Same as above 
    attributeDescriptions[1].location = 1; //Location in shader where data will be read from
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; //Format the data will take (also helps define the size of data)
    attributeDescriptions[1].offset = offsetof(Vertex,col); //Where this attribute is defined in the data for a single vertex
    

    //--Vertex input--
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription; //List of vertex binding descriptions (data spacing/ stride information)
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); //List of vertex attribute descriptions (data format and where to bind to/from)

    //--Input assembly--
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//glTriangles or glLines
    inputAssembly.primitiveRestartEnable = VK_FALSE; //Allow overriding of strip topology to start new primitives

    //--Viewport & scissor --
    //Create  a viewport info struct
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    //Create a scissor info struct
    VkRect2D scissor {};
    scissor.offset ={0,0}; //Offset to use region from 
    scissor.extent = swapChainExtent; //Extent to describe region to use, starting at offset

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    // //--Dynamic state-- alterable things in pipeline
    // std::vector<VkDynamicState> dynamicStatesEnables;
    // dynamicStatesEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT); //Dynamic Viewport : Can resize in command buffer width vkCmdSetViewport(commandbuffer, 0, 1, &viewport)
    // dynamicStatesEnables.push_back(VK_DYNAMIC_STATE_SCISSOR); //Dynamic Scissor : Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0,1, &scissor)
    //
    // //Dynamic state creation info
    // VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    // dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStatesEnables.size());
    // dynamicStateCreateInfo.pDynamicStates = dynamicStatesEnables.data();

    //--Rasterizer--
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE; //Flatten everything beyond the far plane if needed enable in device features depth clamped
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE; //Discard fragment shader data
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL; // How to handle filling points between vertices
    rasterizationStateCreateInfo.lineWidth = 1.0f; //How thick line should be drawn, if needed any other gpu extension is required
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT; //Dont draw the back face
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //*** Vulkan Y direction is inverted
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //Whether to add depth bias to fragments

    //--Multisampling --
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --Blending --
    VkPipelineColorBlendAttachmentState colorState{};
    colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; //Colors to apply blending to
    colorState.blendEnable = VK_TRUE;    //Enable blending

    //Blending uses equation: (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
    colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorState.colorBlendOp = VK_BLEND_OP_ADD;

    colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorState.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo  colorBlendingCreateInfo{};
    colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendingCreateInfo.attachmentCount = 1;
    colorBlendingCreateInfo.pAttachments = &colorState;

    //--Pipeline layout--
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice,&pipelineLayoutCreateInfo,nullptr,&pipelineLayout);

    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    //-Depth stencil testing-
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

    //--Graphics pipeline creation --
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2; //Number of shader stages
    pipelineCreateInfo.pStages = shaderStages; //List of shader stages
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo; //All the fixed function pipeline states
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout; //Pipeline layout pipeline should use 
    pipelineCreateInfo.renderPass = renderPass; // Render pass description the pipeline is compatible with
    pipelineCreateInfo.subpass = 0; //Subpass of render pass to use with pipeline

    //Pipeline derivatives : can create multiple pipelines that derive from one another for optimization
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; //Existing pipeline to derive from...
    pipelineCreateInfo.basePipelineIndex = -1; //or index of pipeline being created to derive from (in case creating multiple ones)

    //Create graphics pieline
    result = vkCreateGraphicsPipelines(mainDevice.logicalDevice,VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,&graphicsPipeline);

    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a graphics pipeline");
    //Destroy modules
    vkDestroyShaderModule(mainDevice.logicalDevice,vertexShaderModule,nullptr);
    vkDestroyShaderModule(mainDevice.logicalDevice,fragmentShaderModule,nullptr);

}

void VulkanRenderer::CreateDepthBufferImage()
{
    VkFormat depthFormat = ChooseSupportedFormat(
        {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    depthBufferImage = CreateImage(swapChainExtent.width,swapChainExtent.height,depthFormat,VK_IMAGE_TILING_OPTIMAL
        , VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&depthBufferImageMemory);

    depthBufferImageView = CreateImageView(depthBufferImage,depthFormat,VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::CreateFramebuffers()
{
    //Resize framebuffer count to equal swap chain image count
    swapchainFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainFramebuffers.size(); ++i)
    {
        std::array<VkImageView, 2> attachments = {
            swapchainImages[i].imageView,
            depthBufferImageView 
        };
        
        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;    //Render pass layout the framebuffer will be used with
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); 
        framebufferCreateInfo.pAttachments = attachments.data(); //list of attachments (1:1 with render pass)
        framebufferCreateInfo.width = swapChainExtent.width; //Framebuffer width
        framebufferCreateInfo.height = swapChainExtent.height; //Framebuffer height
        framebufferCreateInfo.layers = 1; //Framebuffer layers

        VkResult result  = vkCreateFramebuffer(mainDevice.logicalDevice,&framebufferCreateInfo,nullptr, &swapchainFramebuffers[i]);
        if(result != VK_SUCCESS)
            throw std::runtime_error("Failed to create a framebuffer");
    }
}

void VulkanRenderer::CreateCommandPool()
{
    //Get indices of queue families from device
    QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(mainDevice.physicalDevice);
    
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily; //Queue family type that buffers from this command pool will use

    //Create a graphics family command pool
    VkResult result = vkCreateCommandPool(mainDevice.logicalDevice,&poolCreateInfo,nullptr,&graphicsCommandPool);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Command pool");
}

void VulkanRenderer::CreateCommandBuffers()
{
    //Resize command buffer count to have one for each framebuffer
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo cbAllocInfo{};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = graphicsCommandPool;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; //Buffer you submit directly to queue. Cant be called to other buffer
    cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    //Allocate command buffers and place handles in array of buffers
    VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice,&cbAllocInfo,commandBuffers.data());
    if(result != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate Command buffers");   
    }
}

void VulkanRenderer::CreateSynchronisation()
{
    imageAvailable.resize(MAX_FRAME_DRAWS);
    renderFinished.resize(MAX_FRAME_DRAWS);
    drawFences.resize(MAX_FRAME_DRAWS);
    //Semaphore creation information
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    //Fence creation information
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for(size_t i = 0; i< MAX_FRAME_DRAWS; i++)
    {
        if(vkCreateSemaphore(mainDevice.logicalDevice,&semaphoreCreateInfo,nullptr,&imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mainDevice.logicalDevice,&semaphoreCreateInfo,nullptr,&renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(mainDevice.logicalDevice,&fenceCreateInfo, nullptr,&drawFences[i]) != VK_SUCCESS)
                throw std::runtime_error("Error creating semaphores or fence");
    }
}

void VulkanRenderer::CreateUniformBuffers()
{
    //ViewProjection buffer size
    VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

    //Model buffer size
    //VkDeviceSize modelBufferSize = modelUniformAlignment* MAX_OBJECTS;
    
    //One uniform buffer for each image (and by extension, command buffer)
    size_t size = swapchainImages.size();
    vpUniformBuffer.resize(size);
    vpUniformBufferMemory.resize(size);

    //modelUniformBuffer.resize(size);
    //modelUniformBufferMemory.resize(size);

    
    //Create uniform buffers
    for (size_t i = 0; i < size; ++i)
    {
        CreateBuffer(mainDevice.physicalDevice,mainDevice.logicalDevice,vpBufferSize,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&vpUniformBuffer[i],&vpUniformBufferMemory[i]);

        //CreateBuffer(mainDevice.physicalDevice,mainDevice.logicalDevice,modelBufferSize,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&modelUniformBuffer[i],&modelUniformBufferMemory[i]);
    }
}

void VulkanRenderer::CreateDescriptorPool()
{
    //View projection pool
    VkDescriptorPoolSize vpPoolSize{};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

    //Model pool (dynamic)
    /*VkDescriptorPoolSize modelPoolSize{};
    modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelPoolSize.descriptorCount = static_cast<uint32_t>(modelUniformBuffer.size());
    */

    std::vector<VkDescriptorPoolSize> pools ={vpPoolSize};
    
    //Data to create descriptor pool
    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(pools.size());
    poolCreateInfo.pPoolSizes = pools.data();

    //Create descriptor pool
    VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo,nullptr,&descriptorPool);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Error creating descriptor pool");
}

void VulkanRenderer::CreateDescriptorSets()
{
    //Resize descriptor set list so one for every buffer
    descriptorSets.resize(swapchainImages.size());

    std::vector<VkDescriptorSetLayout> setLayouts(vpUniformBuffer.size(),descriptorSetLayout);
    
    VkDescriptorSetAllocateInfo setAllocateInfo{};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = descriptorPool;
    setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
    setAllocateInfo.pSetLayouts = setLayouts.data();

    //Allocate descriptor sets (multiple)
    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice,&setAllocateInfo,descriptorSets.data());
    if(result)
        throw std::runtime_error("Fail to allocate descriptor sets");

    //Update all of descriptor set buffer bindings
    for (size_t i = 0; i < swapchainImages.size(); ++i)
    {
        //View projection descriptor 
        //Buffer info and data offset info
        VkDescriptorBufferInfo vpBufferInfo{};
        vpBufferInfo.buffer = vpUniformBuffer[i]; //Buffer to get data from
        vpBufferInfo.offset = 0;
        vpBufferInfo.range = sizeof(UboViewProjection);
        
        VkWriteDescriptorSet vpSetWrite{};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = descriptorSets[i]; //Descriptor set to update
        vpSetWrite.dstBinding = 0; //Binding to update
        vpSetWrite.dstArrayElement = 0; //Index in array to update
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpSetWrite.descriptorCount = 1;
        vpSetWrite.pBufferInfo = &vpBufferInfo;

        //Model descriptor
        //Model buffer binding info
        /*VkDescriptorBufferInfo modelBufferInfo{};
        modelBufferInfo.buffer = modelUniformBuffer[i];
        modelBufferInfo.offset = 0;
        modelBufferInfo.range = modelUniformAlignment;

        VkWriteDescriptorSet modelSetWrite{};
        modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        modelSetWrite.dstSet = descriptorSets[i];
        modelSetWrite.dstBinding = 1;
        modelSetWrite.dstArrayElement = 0;
        modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        modelSetWrite.descriptorCount = 1;
        modelSetWrite.pBufferInfo = &modelBufferInfo;*/

        //List of descriptor set writes
        std::vector<VkWriteDescriptorSet> setWrites{vpSetWrite};

        //Update the descriptor set with new buffer/binding info
        vkUpdateDescriptorSets(mainDevice.logicalDevice,static_cast<uint32_t>(setWrites.size()),setWrites.data(),0,nullptr);
    }
}

void VulkanRenderer::UpdateUniformBuffer(uint32_t imageIndex)
{

    //Copy VP data
    void* data;
    vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex],0,sizeof(UboViewProjection),0,&data);
    memcpy(data,&uboViewProjection,sizeof(UboViewProjection));
    vkUnmapMemory(mainDevice.logicalDevice,vpUniformBufferMemory[imageIndex]);

    //Copy model data
    // for (size_t i = 0; i < meshList.size(); ++i)
    // {
    //     Model* thisModel = reinterpret_cast<Model*>(reinterpret_cast<uint64_t>(modelTransferSpace) + (i * modelUniformAlignment));
    //     thisModel->currentModel = meshList[i].GetModel();
    // }
  
    // vkMapMemory(mainDevice.logicalDevice, modelUniformBufferMemory[imageIndex],0,modelUniformAlignment*meshList.size(),0,&data);
    // memcpy(data,modelTransferSpace,modelUniformAlignment*meshList.size());
    // vkUnmapMemory(mainDevice.logicalDevice,modelUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::RecordCommands(uint32_t currentImage)
{
    //Information about how to begin each command buffer
    VkCommandBufferBeginInfo bufferBeginInfo{};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; //Buffer can be resubmitted when it has already been submitted and is awaiting execution

    //Information about how to begin a render pass(only needed for graphical applications)
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass; // Render Pass to begin
    renderPassBeginInfo.renderArea.offset = {0,0}; // Start point of render pass in pixels
    renderPassBeginInfo.renderArea.extent = swapChainExtent; //Size of region to run render pass on (starting at offset)
    std::array<VkClearValue,2> clearValues{};
    clearValues[0].color ={0.6f,0.65f,0.4f,1.0f};
    clearValues[1].depthStencil.depth = 1.0f;
    
    renderPassBeginInfo.pClearValues = clearValues.data(); //List of clear values
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    
   
    renderPassBeginInfo.framebuffer = swapchainFramebuffers[currentImage];
    
    //Start recording commands to command buffer!
    VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage],&bufferBeginInfo);
    if(result)
    {
        throw std::runtime_error("Failed to start recording a Command Buffer");
    }

    
    vkCmdBeginRenderPass(commandBuffers[currentImage],&renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

        //Bind pipeline to be used in render pass
        vkCmdBindPipeline(commandBuffers[currentImage],VK_PIPELINE_BIND_POINT_GRAPHICS,graphicsPipeline);

                for(size_t j = 0; j< meshList.size(); j++)
                {
                    VkBuffer vertexBuffers[] = {meshList[j].GetVertexBuffer()}; // Buffers to bind
                    VkDeviceSize offsets[] ={0}; //Offsets into buffers being bound
                    vkCmdBindVertexBuffers(commandBuffers[currentImage],0,1, vertexBuffers,offsets);

                    vkCmdBindIndexBuffer(commandBuffers[currentImage],meshList[j].GetIndexBuffer(),0,VK_INDEX_TYPE_UINT32);

                    //Dynamic offset amount
                    //uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment)*j;

                    vkCmdPushConstants(commandBuffers[currentImage], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                       sizeof(Model), &meshList[j].GetModel());

                    vkCmdBindDescriptorSets(commandBuffers[currentImage],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,
                        0,1,&descriptorSets[currentImage],0,nullptr);

                    //Execute pipeline
                    vkCmdDrawIndexed(commandBuffers[currentImage],meshList[j].GetIndicesCount(),1,0,0,0);
                }
    
    vkCmdEndRenderPass(commandBuffers[currentImage]);

    //Stop recording to command buffer
    result = vkEndCommandBuffer(commandBuffers[currentImage]);
    if(result)
        throw std::runtime_error("Failed to stop recording a Command Buffer");

}

bool VulkanRenderer::CheckInstanceExtensionSupport(const std::vector<const char*>& checkExtensions) const
{
    //Need to get number of extension to create array of correct size to hold extensions
    uint32_t extensionCount  = 0;
    vkEnumerateInstanceExtensionProperties(nullptr,&extensionCount,nullptr);

    //Create a list of vkExtensionProperties using count
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    //Check if given extensions are in list of available extensions
    for (const char* checkExtension: checkExtensions)
    {
        bool hasExtension = false;
        for (const VkExtensionProperties& extension : extensions)
        {
            if(strcmp(checkExtension,extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }
        if(!hasExtension)
        {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CheckDeviceExtensionSupport(const VkPhysicalDevice& device) const
{
    //Get device extension count
    uint32_t extensionCount =0;
    vkEnumerateDeviceExtensionProperties(device, nullptr,&extensionCount,nullptr);

    if(!extensionCount) return false;

    //Populate list of extensions
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr,&extensionCount,extensions.data());

    for (const auto &deviceExtension : deviceExtensions)
    {
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if(strcmp(deviceExtension, extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }

        if(!hasExtension)
        {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CheckDeviceSuitable(const VkPhysicalDevice& device) const
{
    /*//Information about the device itself (ID, name, type, vendor, etc)
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device,&deviceProperties);
   
    //Information about what the device can do (geo shader,tess shader, wide lines, etc.)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device,&deviceFeatures);*/

    QueueFamilyIndices indices = GetQueueFamilies(device);
    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    bool swapChainValid = false;
    if(extensionsSupported)
    {
        SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
    }
  
    return indices.IsValid() && extensionsSupported && swapChainValid;
}

bool VulkanRenderer::CheckValidationLayerSupport() const
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount,nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount,availableLayers.data());

    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers)
        {
            if(strcmp(layerName,layerProperties.layerName))
            {
                layerFound = true;
                break;
            }
        }

        if(!layerFound)
        {
            return false;
        }
    }
    return true;
    
}

void VulkanRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
}

QueueFamilyIndices& VulkanRenderer::GetQueueFamilies(const VkPhysicalDevice& device) const
{
    QueueFamilyIndices indices;

    //GetAll queue family property info for the given device
    uint32_t queueFamilyCount  = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device,&queueFamilyCount,nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device,&queueFamilyCount,queueFamilyProperties.data());


    //Go through each queue family and check if it has at least 1 of the required types of queue
    int i = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilyProperties)
    {
        //Queue can be multiple types defined through bitfield. Need to bitwise AND VK_QUEUE_*_BIT to check if has required one
        if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && indices.graphicsFamily <0)
        {
           indices.graphicsFamily = i;
        }
        
        //Check if queue family supports presentation
        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device,i,surface,&presentationSupport);
        if(queueFamily.queueCount > 0 && presentationSupport && indices.presentationFamily < 0)
        {
            indices.presentationFamily = i;
        }

        if(indices.graphicsFamily >= 0 && indices.presentationFamily >= 0)
            break;
        i++;
    }
    return indices;
    
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(const VkPhysicalDevice& device) const
{
    SwapChainDetails swapChainDetails;
    //--Capabilities--
    //Get the surface capabilities for the given surface on the given physical device
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device,surface,&swapChainDetails.surfaceCapabilities);

    //--Formats --
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device,surface,&formatCount,nullptr);

    if(formatCount)
    {
        swapChainDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device,surface,&formatCount,swapChainDetails.formats.data());
    }

    // -- Presentation modes --
    uint32_t presentationCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,&presentationCount, nullptr);

    if(presentationCount)
    {
        swapChainDetails.presentationModes.resize(presentationCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,&presentationCount, swapChainDetails.presentationModes.data());
    }
    
    return swapChainDetails;
}


std::optional<VkSurfaceFormatKHR> VulkanRenderer::ChooseBestSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) const
{
    //If only 1 format available and is undefined , then this means all formats are available. 
    if(formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        return std::optional<VkSurfaceFormatKHR>({VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR});

    //If restricted, search for optimal format
    for (const auto& format : formats)
    {
        if((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)  && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
            
    }
    return std::nullopt;
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(
    const std::vector<VkPresentModeKHR>& presentationModes) const
{
    for (const auto& presentationMode : presentationModes)
    {
        if(presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentationMode;
    }
    return VK_PRESENT_MODE_FIFO_KHR; //This always has to be available
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const
{
    //If current extent is at numeric limits then extent cant vary. 
    if(surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return surfaceCapabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window,&width,&height);

    //Create new extent using window size
    VkExtent2D newExtent{};
    newExtent.width = static_cast<uint32_t>(width);
    newExtent.height = static_cast<uint32_t>(height);

    //Surface also defines max and min, so make sure within boundaries by clamping value.
    newExtent.width =std::max(surfaceCapabilities.minImageExtent.width,std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
    newExtent.width =std::max(surfaceCapabilities.minImageExtent.height,std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));
    return newExtent;
}

VkFormat VulkanRenderer::ChooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling,
    VkFormatFeatureFlags featureFlags) const
{
    for (const VkFormat& format : formats)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice,format,&properties);

        if(tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags)== featureFlags)
        {
            return format;
        }
        else
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags)== featureFlags)
            {
                return format;
            }
    }

    throw std::runtime_error("Failed to find a matching format");
}

VkImage VulkanRenderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                    VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = useFlags;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    VkResult result  = vkCreateImage(mainDevice.logicalDevice,&imageCreateInfo, nullptr,&image);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create an image");

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(mainDevice.logicalDevice,image,&memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(mainDevice.physicalDevice,memoryRequirements.memoryTypeBits,propFlags);

    result = vkAllocateMemory(mainDevice.logicalDevice,&memoryAllocateInfo,nullptr,imageMemory);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate memory for image");

    //Connect memory to image
    vkBindImageMemory(mainDevice.logicalDevice,image,*imageMemory,0);

    return image;
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const
{
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image; //Image to create view for
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components.r  = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g  = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b  = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a  = VK_COMPONENT_SWIZZLE_IDENTITY;

    //Sub-resources allow the view to view only a part of an image
    viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
    viewCreateInfo.subresourceRange.baseMipLevel = 0; //Start mipmap level to view from
    viewCreateInfo.subresourceRange.levelCount = 1; // Number of mipmap levels to view
    viewCreateInfo.subresourceRange.baseArrayLayer = 0; // Start array level to view from
    viewCreateInfo.subresourceRange.layerCount = 1; // Number of array levels to view
 
    //Create image view and return it
    VkImageView imageView;
    VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
    if(result != VK_SUCCESS)
    throw std::runtime_error("Failed to create an image view");

    return imageView;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(mainDevice.logicalDevice,&shaderModuleCreateInfo,nullptr,&shaderModule);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Error creating shader module");

    return shaderModule;
}


stbi_uc* VulkanRenderer::LoadTextureFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize) const
{
    //Number of channels image uses
    int channels;

    //Load pixel data for image
    std::string fileLoc = "Textures/"+fileName;
    stbi_uc* image = stbi_load(fileLoc.c_str(),width,height,&channels,STBI_rgb_alpha);

    if(!image)
        throw std::runtime_error("Failed to load a texture file "+fileName);

    *imageSize = *width* *height *4; //4 is all the channels

    return image;
}

int VulkanRenderer::CreateTexture(const std::string& fileName)
{
    //Load image file
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc* imageData = LoadTextureFile(fileName,&width,&height,&imageSize);

    //Create staging buffer to hold loaded data, ready to copy to device
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;
    CreateBuffer(mainDevice.physicalDevice,mainDevice.logicalDevice,imageSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &imageStagingBuffer,&imageStagingBufferMemory);

    //Copy image data to staging buffer
    void* data;
    vkMapMemory(mainDevice.logicalDevice,imageStagingBufferMemory,0,imageSize,0,&data);
    memcpy(data,imageData,static_cast<size_t>(imageSize));
    vkUnmapMemory(mainDevice.logicalDevice,imageStagingBufferMemory);

    //Free original image data
    stbi_image_free(imageData);

    //Create image to hold final texture
    VkImage texImage;
    VkDeviceMemory texImageMemory;

    texImage = CreateImage(width,height,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);
    
    //Copy data to image

    TransitionImageLayout(mainDevice.logicalDevice,graphicsQueue,graphicsCommandPool,texImage,
        VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    CopyImageBuffer(mainDevice.logicalDevice,graphicsQueue,graphicsCommandPool,imageStagingBuffer,texImage,width,height);

    TransitionImageLayout(mainDevice.logicalDevice,graphicsQueue,graphicsCommandPool,texImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //Add texture data to vector for reference
    textureImages.push_back(texImage);
    textureImageMemory.push_back(texImageMemory);

    //Destroy staging buffers
    vkDestroyBuffer(mainDevice.logicalDevice,imageStagingBuffer,nullptr);
    vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory,nullptr);

    return textureImages.size()-1;
}

void VulkanRenderer::Cleanup() 
{
    
    vkDeviceWaitIdle(mainDevice.logicalDevice);

    //_aligned_free(modelTransferSpace);

    for (size_t i = 0; i < textureImages.size(); ++i)
    {
        vkDestroyImage(mainDevice.logicalDevice, textureImages[i],nullptr);
        vkFreeMemory(mainDevice.logicalDevice,textureImageMemory[i],nullptr);
    }
    
    vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView,nullptr);
    vkDestroyImage(mainDevice.logicalDevice,depthBufferImage,nullptr);
    vkFreeMemory(mainDevice.logicalDevice,depthBufferImageMemory,nullptr);
    
    vkDestroyDescriptorPool(mainDevice.logicalDevice,descriptorPool,nullptr);
    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice,descriptorSetLayout,nullptr);
    for (size_t i = 0; i< swapchainImages.size(); i++)
    {
        vkFreeMemory(mainDevice.logicalDevice,vpUniformBufferMemory[i],nullptr);
        vkDestroyBuffer(mainDevice.logicalDevice,vpUniformBuffer[i],nullptr);

        //vkFreeMemory(mainDevice.logicalDevice,modelUniformBufferMemory[i],nullptr);
        //vkDestroyBuffer(mainDevice.logicalDevice,modelUniformBuffer[i],nullptr);
    }

    for (Mesh& mesh : meshList)
        mesh.DestroyBuffers();

    for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
    {
        vkDestroySemaphore(mainDevice.logicalDevice,renderFinished[i],nullptr);
        vkDestroySemaphore(mainDevice.logicalDevice,imageAvailable[i],nullptr);
        vkDestroyFence(mainDevice.logicalDevice,drawFences[i], nullptr);
    }

    vkDestroyCommandPool(mainDevice.logicalDevice,graphicsCommandPool,nullptr);

    for (auto framebuffer : swapchainFramebuffers)
    {
        vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer,nullptr);
    }

    vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(mainDevice.logicalDevice,pipelineLayout,nullptr);
    vkDestroyRenderPass(mainDevice.logicalDevice,renderPass,nullptr);

    for (auto image : swapchainImages)
    {
        vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
    }

    vkDestroySwapchainKHR(mainDevice.logicalDevice,swapchainKhr,nullptr);
    vkDestroyDevice(mainDevice.logicalDevice,nullptr);
    vkDestroySurfaceKHR(instance,surface,nullptr);
    vkDestroyInstance(instance,nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
    Cleanup();
}
