#include <iostream>
#include <cassert>

#include <GLFW/glfw3.h>

#if 1
#include <vulkan/vulkan.h>
#else
// A bit slow
#include <vulkan/vulkan.hpp>
#endif

using namespace std;

#define VK_CHECK(call) \
	do { \
		VkResult result = call; \
		assert(result == VK_SUCCESS); \
	} while (0)

int main()
{
	cout << "Hello, Vulkan!" << endl;

	int rc = glfwInit();
	if (rc == GLFW_FALSE) 
	{
		cout << "GLFW init failed!" << endl;
		return -1;
	}

	// In real Vulkan applications you should probably check if 1.3 is available via
	// vkEnumerateInstanceVersion(...)
	VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	appInfo.apiVersion = VK_VERSION_1_3;

	const char* layers[] = 
	{
#ifdef _DEBUG
		""
#endif
	};

	VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.pApplicationInfo = &appInfo;
	/*createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = L"dfd";*/

	VkInstance instance;
	VK_CHECK( vkCreateInstance(&createInfo, nullptr, &instance) );
	
	GLFWwindow *window = glfwCreateWindow(1024, 768, "Niagara", 0, 0);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	vkDestroyInstance(instance, nullptr);

	return 0;
}
