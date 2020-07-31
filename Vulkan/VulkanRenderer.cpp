#include "VulkanRenderer.h"

#include <algorithm>
#include <array>


#include "Utilities.h"
#include <iostream>
#include <optional>
#include <vector>
#include <set>
#include <stdbool.h>


VulkanRenderer::VulkanRenderer():
    window(nullptr), instance(nullptr),
    mainDevice(), graphicsQueue(nullptr),
    presentationQueue(nullptr), surface(nullptr),
    swapchainKhr(nullptr)
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
        CreateGraphicsPipeline();
    }
    catch (const std::runtime_error& e)
    {
        printf("Error : %s  \n",e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
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
}

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

    //Attachment reference uses an attachment index tha refers to index in the attachment list passed to renderPassCreateInfo
    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //Information about a particular subpass the render pass is using
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; //Pipeline type subpass is to be bound to
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

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

    //Create info for render pass
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(mainDevice.logicalDevice,& renderPassCreateInfo,nullptr,&renderPass);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Error creating render pass");
    
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
    //--Vertex input--
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr; //List of vertex binding descriptions (data spacing/ stride information)
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr; //List of vertex attribute descriptions (data format and where to bind to/from)

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
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pPushConstantRanges = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice,&pipelineLayoutCreateInfo,nullptr,&pipelineLayout);

    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    //-Depth stencil testing-
    //TODO: set up depth stencil testing

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
    pipelineCreateInfo.pDepthStencilState = nullptr;
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

void VulkanRenderer::CreateFramebuffers()
{
    //Resize framebuffer count to equal swap chain image count
    swapchainFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainFramebuffers.size(); ++i)
    {
        std::array<VkImageView, 1> attachments = {
            swapchainImages[i].imageView
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

    //Subresources allow the view to view only a part of an image
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


void VulkanRenderer::Cleanup() const 
{
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
