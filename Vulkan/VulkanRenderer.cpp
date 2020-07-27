#include "VulkanRenderer.h"
#include "Utilities.h"
#include <iostream>
#include <vector>
#include <set>
#include <stdbool.h>


VulkanRenderer::VulkanRenderer():
    window(nullptr), instance(nullptr),
    mainDevice(), graphicsQueue(nullptr)
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

bool VulkanRenderer::CheckInstanceExtensionSupport(const std::vector<const char*>& checkExtensions) const
{
    //Need to get number of extension to create array of correct size to hold extensions
    uint32_t extensionCount  = 0;
    vkEnumerateInstanceExtensionProperties(nullptr,&extensionCount,nullptr);

    //Create a list of vkExtensionProperties using count
    //std::vector<VkExtensionProperties> extensions(extensionCount);
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
  
    return indices.IsValid() && extensionsSupported;
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



void VulkanRenderer::Cleanup() const 
{
    vkDestroyDevice(mainDevice.logicalDevice,nullptr);
    vkDestroySurfaceKHR(instance,surface,nullptr);
    vkDestroyInstance(instance,nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
    Cleanup();
}
