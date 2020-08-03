#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>

#include "VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;

void InitWindow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	if (!glfwInit())
	{
		std::cout<< "GLFW Not initialized"<<std::endl;
		return;
	}
	//Set GLFW to not work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width,height,wName.c_str(),nullptr,nullptr);
}

int main()
{
	//Create Window
	InitWindow();

	//Create vulkan renderer instance
	if(vulkanRenderer.Init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}
	
	//Loop until closed
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		vulkanRenderer.Draw();
	}

	//Destroy window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();
		
	
	return 0;
}