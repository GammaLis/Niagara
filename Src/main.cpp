#include <iostream>
#include <vector>
#include <set>
#include <cassert>
#include <optional>

#if 1
#define GLFW_INCLUDE_VULKAN
//#if defined(GLFW_INCLUDE_VULKAN)
//#include <vulkan/vulkan.h>
//#endif /* Vulkan header */
#endif

#ifndef GLFW_INCLUDE_VULKAN

#if 1
#include <vulkan/vulkan.h>
#else
// A bit slow
#include <vulkan/vulkan.hpp>
#endif

#endif

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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


bool CheckValidationLayerSupport(const std::vector<const char*> &validationLayers) 
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

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

	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG
	const bool bEnableValidationLayers = false;
#else
	const bool bEnableVadialtionLayers = true;
#endif

	assert(!bEnableVadialtionLayers || CheckValidationLayerSupport(validationLayers));

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
		if (bEnableVadialtionLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else 
		{
			createInfo.enabledLayerCount = 0;
		}
#if 0
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();
#else
		createInfo.ppEnabledExtensionNames = glfwExtensions;
		createInfo.enabledExtensionCount = glfwExtensionCount;
#endif

		VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
	}

	return instance;
}

/**
* Selecting a physical device
* After initializing the Vulkan library through a `VkInstance` we need to look for and select a graphics card 
* in the system that supports the features we need.
*/

/**
* Anything from drawing to uploading textures, requires commands to be submitted to a queue. There are different types of queues
* that originate from different queue families and each famlity of queues allows only a subset of commands. For example, there could
* be a queue family that only allows processing of compute commands or one that only allows memory transfer related commands.
*/
struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	// It's actually possible that the queue families supporting drawing commands and the ones supporting presentation do not overlap.
	// Therefore we have to take into account that there could be a dinstinct presentation queue.
	std::optional<uint32_t> presentFamily;

	bool IsComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphicsFamily = i;

		// Ensure a device can present images to the surface we created
		VkBool32 presentSupport = 0;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport)
			indices.presentFamily = i;

		if (indices.IsComplete())
			break;

		i++;
	}

	return indices;
}

bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceProperties(device, &props);
	vkGetPhysicalDeviceFeatures(device, &features);

	bool bValid = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader;

	if (bValid)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device, surface);
		bValid = indices.IsComplete();
	}

#ifndef NDEBUG
	if (bValid) 
	{
		std::cout << "GPU: " << props.deviceName << " is valid.\n";
	}
#endif

	return bValid;
}

int RateDeviceSuitability(VkPhysicalDevice device)
{
	int score = 0;

	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceProperties(device, &props);
	vkGetPhysicalDeviceFeatures(device, &features);

	// Discrete GPUs have a significant performance advantage
	if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	// Maximum possible size of textures affects graphics quality
	score += props.limits.maxImageDimension2D;

	// Application can't function without geometry shaders
	if (!features.geometryShader)
		score = 0;

	return score;
}

VkPhysicalDevice GetPhysicalDevice(VkPhysicalDevice *phsicalDevices, uint32_t physicalDeviceCount, VkSurfaceKHR surface)
{
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		if (IsDeviceSuitable(phsicalDevices[i], surface)) 
		{
			physicalDevice = phsicalDevices[i];
			break;
		}
	}

	assert(physicalDevice != VK_NULL_HANDLE);

	return physicalDevice;
}

VkPhysicalDevice GetPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	assert(deviceCount > 0);

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	return GetPhysicalDevice(physicalDevices.data(), deviceCount, surface);
}

VkDevice GetLogicalDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices &indices) 
{
	// Specifying the queues to be created
	// This structure describes the number of queues we want for a single queue family.

	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
#if 0
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	queueCreateInfos.push_back(queueCreateInfo);

#else
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos.push_back(queueCreateInfo);
	}
#endif

	// Specifying used device features
	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	// The `enabledLayerCount` and `ppEnabledLayerNames` fields of `VkDeviceCreateInfo` are ignored by up-to-date implementations.

	VkDevice device = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

	return device;
}

VkSurfaceKHR GetWindowSurface(VkInstance instance, GLFWwindow *window)
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;

#if 0
	VkWin32SurfaceCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = glfwGetWin32Window(window);
	createInfo.hinstance = GetModuleHandle(nullptr);

	
	VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface));

#else
	VK_CHECK( glfwCreateWindowSurface(instance, window, nullptr, &surface) );
#endif

	return surface;
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
	// Because GLFW was originally designed to create an OpenGL context, we need to tell it to not create an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Niagara", nullptr, nullptr);

	VkInstance instance = GetVulkanInstance();
	assert(instance);

	VkSurfaceKHR surface = GetWindowSurface(instance, window);
	assert(surface);

	VkPhysicalDevice physicalDevice = GetPhysicalDevice(instance, surface);
	assert(physicalDevice);

	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);

	VkDevice device = GetLogicalDevice(physicalDevice, indices);
	assert(device);

	// Retrieving queue handles
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	// We are only creating a single queue from this family, we'll simply use index 0
	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	assert(graphicsQueue);

	// Creating the presentation queue
	VkQueue presentQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	assert(presentQueue);
	
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
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
