#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

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

    //-Support functions
    bool CheckInstanceExtensionSupport(const std::vector<const char*>& checkExtensions);

    //- Destroy functions
    void Cleanup() const;
    
};
