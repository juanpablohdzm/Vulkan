#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <glm/ext/matrix_transform.hpp>


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

	float angle = 0.0f;
	float dt = 0.0f;
	float lastTime = 0.0f;
	
	//Loop until closed
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		dt = now - lastTime;
		lastTime = now;
		angle += 20.0f*dt;
		if(angle >= 360.0f)
			angle = 0.0f;


		glm::mat4 model(1.0f);
		glm::mat4 model2(1.0f);
		model = glm::scale(model,glm::vec3(0.5,0.5,0.5));
		model = glm::translate(model,glm::vec3(1.0f,0.0f,0.0f));
		model = glm::rotate(model,glm::radians(angle),glm::vec3(0.0f,0.0f,1.0f));
		model2 = glm::scale(model2,glm::vec3(0.5,0.5,0.5));
		model2 = glm::translate(model2,glm::vec3(-1.0f,0.0f,0.0f));
		model2 = glm::rotate(model2,glm::radians(-angle),glm::vec3(0.0f,0.0f,1.0f));

		vulkanRenderer.UpdateModel(0,model);
		vulkanRenderer.UpdateModel(1,model2);
		vulkanRenderer.Draw();
	}

	//Destroy window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();
		
	
	return 0;
}