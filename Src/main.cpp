#include <iostream>
#include <vector>
#include <cassert>

#if 0
#define GLFW_INCLUDE_VULKAN
#if defined(GLFW_INCLUDE_VULKAN)
#include <vulkan/vulkan.h>
#endif /* Vulkan header */
#endif
#include <GLFW/glfw3.h>

#if 1
#include <vulkan/vulkan.h>
#else
// A bit slow
#include <vulkan/vulkan.hpp>
#endif

using namespace std;

/**
* What it takes to draw a triangle
* 1. Instance and physical device selection
* 2. Logical devicen and queue families
* 3. Window surface and swap chain
* 4. Image views and framebuffers
* 5. Render passes
* 6. Graphics pipeline
* 7. Command pools and command buffers
* 8. Main loop
* 
* Specifically,
* * Create a `VkInstance`
* * Select a supported graphics card (`VkPhysicalDevice`)
* * Create a `VkDevice` and `VkQueue` for drawing and presentation
* * Create a window, window surface and swap chain
* * Wrap the swap chain images into `VkImageView`
* * Create a render pass that specifies the render targets and usage
* * Create the framebuffers for the render pass
* * Set up the graphics pipeline
* * Allocate and record a command buffer with the draw commands for every possible swap chain image 
* * Draw frames by aquiring images, submitting the right draw command buffers and returing the images back to the swap chain
*/

/**
* Coding conventions
* Functions have a lower case `vk` prefix, types like enumerations and structs have a `Vk` prefix and enumeration values have a
* `VK_` prefix. The API heavily uses structs to provide parameters to functions.
* VkXXXCreateInfo createInfo;
* createInfo.sType = VK_STRUCTURE_TYPE_XXX_CREATE_INFO;
* createInfo.pNext = nullptr;
* createInfo.foo = ...;
* createInfo.bar = ...;
* Many structures in Vulkan requre you to explicitly specify the type of structure in the `sType` member. The `pNext` member
* can point to an extension structure and will always be `nullptr` in this tutorial.
* Almost all functions return a `VkResult` that is either `VK_SUCCESS` or an error code.
*/

#define VK_CHECK(call) \
	do { \
		VkResult result = call; \
		assert(result == VK_SUCCESS); \
	} while (0)


// The very first thing you need to do is initialize the Vulkan library by creating an instance. 
// The instance is the connection between your application and the Vulkan library and creating it 
// involves specifying some details about your application to the driver.
VkInstance GetVulkanInstance()
{
	// In real Vulkan applications you should probably check if 1.3 is available via
	// vkEnumerateInstanceVersion(...)
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	// Using value initialization to leave it as `nullptr`
	// appInfo.pNext = nullptr;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG
	const bool bEnableValidationLayers = false;
#else
	const bool bEnableVadialtionLayers = true;
#endif

	std::vector<const char*> extensions = {
		VK_KHR_SURFACE_EXTENSION_NAME
	};

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	VkInstance instance = VK_NULL_HANDLE;
	{
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
		createInfo.ppEnabledLayerNames = validationLayers.data();
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
	}

	return instance;
}

VkPhysicalDevice GetPhysicalDevice(VkPhysicalDevice phsicalDevices[], uint32_t physicalDeviceCount)
{
	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phsicalDevices[i], &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			cout << "Picking discrete GPU: " << props.deviceName << endl;
			return phsicalDevices[i];
		}
	}

	if (physicalDeviceCount > 0)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phsicalDevices[0], &props);
		cout << "Picking fallback GPU: " << props.deviceName << endl;
		return phsicalDevices[0];
	}

	cout << "No physical device available!" << endl;
	return VK_NULL_HANDLE;

}


int main()
{
	cout << "Hello, Vulkan!" << endl;

	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	int rc = glfwInit();
	if (rc == GLFW_FALSE) 
	{
		cout << "GLFW init failed!" << endl;
		return -1;
	}

	VkInstance instance = GetVulkanInstance();
	assert(instance);

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; 
	{
		constexpr uint32_t MaxDeviceCount = 8;
		VkPhysicalDevice physicalDevices[MaxDeviceCount];
		uint32_t physicalDeviceCount = MaxDeviceCount;
		VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

		physicalDevice = GetPhysicalDevice(physicalDevices, physicalDeviceCount);
		assert(physicalDevice);
	}

	VkDevice device = VK_NULL_HANDLE;
	{
		VkDeviceCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		// vkCreateDevice(physicalDevice, )
	}
	
	// Because GLFW was originally designed to create an OpenGL context, we need to tell it to not create an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Niagara", nullptr, nullptr);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	vkDestroyInstance(instance, nullptr);

	return 0;
}

/**
* 1. Instance and physical device selection
* A Vulkan application starts by setting up the Vulkan API through a `VkInstance`. An instance is created by describing your application and
* any API extensions you will be using. After creating the instance, you can query for Vulkan supported hardware and select one or more
* `VkPhysicalDevice`s to use for operations.
* 
* 2. Logical device and queue families
* After selecting the right hardware device to use, you need to create a `VkDevice`(logical device), where you describe more specifically
* which `VkPhysicalDeviceFeatures` you will be using, like multi viewport rendering and 64 bit floats. You also need to specify
* which queue families you would like to use. Most operations performed with Vulkan, like draw commands and memory operations,
* are asynchronously executed by submitting them to a `VkQueue`. Queues are allocated from queue familites, where each queue family
* supports a specific set of operations in its queues. For example, there could be separate queue families for graphics, compute and
* memory transfer operations.
* 
* 3. Window surface and swap chain
* We need to create a window to present rendered images to. And we need 2 more components to actually render to a window: 
* a window surface (`VkSurfaceKHR`) and a swap chain (`VkSwapchainKHR`). The surface is a cross-platform abstraction over windows
* to render to and is generally instantiated by providing a reference to the native window handle, for example `HWND` on Windows.
* The swap chain is a collection of render targets. Its basic purpose is to ensure that the image that we're currently rendering to
* is different from the one that is currently on the screen. This is important to make sure that only complete images are shown.
* 
* 4. Image views and framebuffers
* To draw to an image acquired from the swap chain, we have to wrap it into a `VkImageView` and `VkFramebuffer`. 
* An image view references a specific part of an image to be used, and a framebuffer references image views that are to be used 
* for color, depth and stencil targets.
* 
* 5. Render passes
* Render passes in Vulkan describe the type of images that are used during rendering operations, how they will be used, and 
* how their contents should be treated.
* 
* 6. Graphics pipeline
* The graphics pipeline in Vulkan is set up by creating a `VkPipeline` object. It describes the configurable state of the graphics 
* card, like the viewport and depth buffer operation and the programmable state using `VkShaderModule` objects. The `VkShaderModule`
* objects are created from shader byte code.
* One of the most distinctive features of Vulkan compared to existing APIs, is that almost all configuration of the graphics pipeline
* needs to be set in advance. That means that if you want to switch to a different shader or slightly change your vertex layout, 
* then you need to entirely recreate the graphics pipeline. That means you will have to create many `VkPipeline` objects in advance
* for all the different combinations you need for your rendering operations. Only some basic configuration, like viewport size and
* clear color, can be changed dynamically.
* 
* 7. Command pools and command buffers
* The drawing operations first need to be recorded into a `VkCommandBuffer` before they can be submitted. These command buffers are
* allocated from a `VkComomandPool` that is associated with a specific queue family. To draw a simple triangle, we need to 
* record a command buffer with the following operations:
* * Begin the render pass
* * Bind the graphics pipeline
* * Draw 3 vertices
* * End the render pass
* 
* 8. Main loop
* Now that the drawing commands have been wrapped into a comand buffer, the main loop is quite straightforward. We first acquire
* an image from the swap chain with `vkAcquireNextImageKHR`. We can then select the appropriate command buffer for that image and
* execute it with `vkQueueSubmit`. Finally, we return the image to the swap chain for presentation to the screen with `vkQueuePresentKHR`.
*/
