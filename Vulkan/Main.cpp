#define GLFW_INCLUDE_VULKAN
/*Adding this glfw handles including the vulkan library*/
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//Vulkan uses depth from -1 to 1
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Test window", nullptr, nullptr);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	const glm::mat4 testMatrix(1.0f);
	const glm::vec4 testVector(1.0f);

	auto testResult  = testMatrix*testVector;

	std::cout << "Extension count: " << extensionCount << std::endl;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	glfwTerminate();
}