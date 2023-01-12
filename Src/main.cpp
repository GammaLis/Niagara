// Ref: https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/
// Ref: https://github.com/KhronosGroup/Vulkan-Samples

#include "pch.h"
#include "Device.h"
#include "Swapchain.h"
#include "Shaders.h"
#include "Pipeline.h"
#include "Utilities.h"

#include <iostream>
#include <fstream>
#include <queue>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace std;

/// Mesh
#include "meshoptimizer.h"

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

/// Custom settings
#define DRAW_SIMPLE_TRIANGLE 0
#define DRAW_SIMPLE_MESH 1

#define DRAW_MODE DRAW_SIMPLE_MESH

#define VERTEX_INPUT_MODE 1
#define FLIP_VIEWPORT 1
#define USE_DEVICE_8BIT_16BIT_EXTENSIONS 1
#define USE_MESHLETS 1
// Mesh shader needs the 8bit_16bit_extension


/// Global variables

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

constexpr uint32_t MESHLET_MAX_VERTICES = 64;
constexpr uint32_t MESHLET_MAX_PRIMITIVES = 84;

const std::string g_ResourcePath = "../Resources/";

bool g_FramebufferResized = false;
GLFWwindow* g_Window = nullptr;

// Window
static void FramebufferResizeCallback(GLFWwindow * window, int width, int height)
{
	g_FramebufferResized = true;
}


// Vulkan
#ifdef NDEBUG
const bool g_bEnableValidationLayers = false;
#else
const bool g_bEnableValidationLayers = true;
#endif

VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
VkDevice g_Device = VK_NULL_HANDLE;

VkCommandPool g_CommandPool = VK_NULL_HANDLE;

// How many frames 
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
// We choose the number of 2 because we don't want the CPU to get too far ahead of the GPU. With 2 frames in flight, the CPU and GPU
// can be working on their own tasks at the same time. If the CPU finishes early, it will wait till the GPU finishes rendering before
// submitting more work.
// Each frame should have its own command buffer, set of semaphores, and fence.

static const uint32_t g_BufferSize = 128 * 1024 * 1024;

const std::vector<const char*> g_InstanceExtensions =
{
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
};

const std::vector<const char*> g_DeviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,

#if USE_MESHLETS
	VK_NV_MESH_SHADER_EXTENSION_NAME,
	// VK_EXT_MESH_SHADER_EXTENSION_NAME
#endif

};

std::vector<VkDynamicState> g_DynamicStates =
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};


/// Declarations

uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties &memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);


/// Structures

/// Command manager
namespace Niagara
{
	class CommandManager
	{
	public:
		static VkCommandBuffer CreateCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = commandPool;
			allocInfo.level = level;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

			return commandBuffer;
		}

		void Init(const Device& device)
		{
			commandPool = device.CreateCommandPool(device.queueFamilyIndices.graphics);

			vkGetDeviceQueue(device, device.queueFamilyIndices.graphics, 0, &graphicsQueue);
			vkGetDeviceQueue(device, device.queueFamilyIndices.compute, 0, &computeQueue);
			vkGetDeviceQueue(device, device.queueFamilyIndices.transfer, 0, &transferQueue);
		}

		void Cleanup(const Device& device)
		{
			vkDestroyCommandPool(device, commandPool, nullptr);
		}

		VkCommandBuffer GetCommandBuffer(const Device& device)
		{
			VkCommandBuffer cmd = CreateCommandBuffer(device, commandPool);
			return cmd;
		}

		VkQueue graphicsQueue;
		VkQueue computeQueue;
		VkQueue transferQueue;

		VkCommandPool commandPool;

	private:
		std::queue<VkCommandBuffer> m_CommandBuffers;
	};
}

Niagara::CommandManager g_CommandMgr{};

struct Vertex
{
#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
	glm::u16vec3 p;
	glm::u8vec4 n;
	glm::u16vec2 uv;
#else
	glm::vec3 p;
	glm::vec3 n;
	glm::vec2 uv;
#endif

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		// A vertex binding describes at which rate to load data from memory throughout the vertices.
		// It specifies the number of bytes between data entries and whether to move to the next data entry after 
		// each vertex or after each instance.
		VkVertexInputBindingDescription desc{};
		// All of the per-vertex data is packed together in one array, so we're only going to have 1 binding.
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescs{};

		// float	VK_FORAMT_R32_SFLOAT
		// vec2		VK_FORMAT_R32G32_SFLOAT
		// vec3		VK_FORMAT_R32G32B32_SFLOAT
		// vec4		VK_FORMAT_R32G32B32A32_SFLOAT
		// ivec2	VK_FORMAT_R32G32_SINT
		// uvec4	VK_FORMAT_R32G32B32A32_UINT
		// double	VK_FORMAT_R64_SFLOAT

		uint32_t vertexAttribOffset = 0;

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
		attributeDescs[0].binding = 0;
		attributeDescs[0].format = VK_FORMAT_R16G16B16_SFLOAT;
		attributeDescs[0].location = 0;
		attributeDescs[0].offset = offsetof(Vertex, p); // vertexAttribOffset 
		vertexAttribOffset += 3 * 2;

		attributeDescs[1].binding = 0;
		attributeDescs[1].format = VK_FORMAT_R8G8B8A8_UINT;
		attributeDescs[1].location = 1;
		attributeDescs[1].offset = offsetof(Vertex, n); // vertexAttribOffset;
		vertexAttribOffset += 4;

		attributeDescs[2].binding = 0;
		attributeDescs[2].format = VK_FORMAT_R16G16_SFLOAT;
		attributeDescs[2].location = 2;
		attributeDescs[2].offset = offsetof(Vertex, uv); // vertexAttribOffset;
		vertexAttribOffset += 2 * 2;

#else
		attributeDescs[0].binding = 0;
		attributeDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescs[0].location = 0;
		attributeDescs[0].offset = offsetof(Vertex, p); // vertexAttribOffset 
		vertexAttribOffset += 3 * 4;

		attributeDescs[1].binding = 0;
		attributeDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescs[1].location = 1;
		attributeDescs[1].offset = offsetof(Vertex, n); // vertexAttribOffset;
		vertexAttribOffset += 3 * 4;

		attributeDescs[2].binding = 0;
		attributeDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescs[2].location = 2;
		attributeDescs[2].offset = offsetof(Vertex, uv); // vertexAttribOffset;
		vertexAttribOffset += 2 * 4;
#endif

		return attributeDescs;
	}
};

/**
* Meshlets
* Each meshlet represents a variable number of vertices and primitives. There are no restrictions regarding the connectivity of
* these primitives. However, they must stay below a maximum amount, specified within the shader code.
* We recommend using up to 64 vertices and 126 primitives. The `6` in 126 is not a typo. The first generation hardware allocates
* primitive indices in 128 byte granularity and needs to reserve 4 bytes for the primitive count. Therefore 3 * 126 + 4 maximizes
* the fit into a 3 * 128 = 384 bytes block. Going beyond 126 triangles would allocate the next 128 bytes. 84 and 40 are other 
* maxima that work well for triangles.
*/
struct Meshlet
{
	glm::vec4 cone;
	uint32_t vertices[MESHLET_MAX_VERTICES];
	uint8_t indices[MESHLET_MAX_PRIMITIVES*3]; // up to MESHLET_MAX_PRIMITIVES triangles
	uint8_t vertexCount = 0;
	uint8_t triangleCount = 0;
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Meshlet> meshlets;
};

struct GpuBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* data = nullptr;
	size_t size = 0;
	uint32_t stride = 0;
	uint32_t elementCount = 0;

	static void Copy(const Niagara::Device &device, GpuBuffer &dstBuffer, const GpuBuffer &srcBuffer, VkDeviceSize size)
	{
		VkCommandBuffer cmd = g_CommandMgr.GetCommandBuffer(device);

		VkCommandBufferBeginInfo cmdBeginInfo{};
		cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(cmd, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);

		VK_CHECK(vkEndCommandBuffer(cmd));

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		vkQueueSubmit(g_CommandMgr.transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		// Unlike the draw commands, there are no events we need to wait on this time. We just want to execute the transfer on the buffers immediately.
		// We could use a fence and wait with `vkWaitForFences`, or simply wait for the transfer queue to become idle with `vkQueueWaitIdle`. A fence would allow
		// you to schedule multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time. That may give
		// the driver more opportunities to optimize.
		vkQueueWaitIdle(g_CommandMgr.transferQueue);

		vkFreeCommandBuffers(device, g_CommandMgr.commandPool, 1, &cmd);
	}

	void Init(const Niagara::Device &device, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags, const void *pInitialData = nullptr)
	{
		VkBufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;
		createInfo.usage = usage;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Optional

		buffer = VK_NULL_HANDLE;
		VK_CHECK(vkCreateBuffer(device, &createInfo, nullptr, &buffer));

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(device.memoryProperties, memRequirements.memoryTypeBits, memoryFlags);

		memory = VK_NULL_HANDLE;
		VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));

		VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

		data = nullptr;
		if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			vkMapMemory(device, memory, 0, size, 0, &data);
		}

		this->size = size;

		if (pInitialData != nullptr)
			Update(device, pInitialData, size);
	}

	void Update(const Niagara::Device &device, const void *pData, size_t size)
	{
		if (buffer == VK_NULL_HANDLE || pData == nullptr) 
			return;

		if (data != nullptr)
		{
			memcpy_s(data, size, pData, size);
		}
		else
		{
			GpuBuffer scratchBuffer{};
			scratchBuffer.Init(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pData);

			Copy(device, *this, scratchBuffer, size);

			scratchBuffer.Destory(device);
		}
	}	

	void Destory(const Niagara::Device &device)
	{
		vkFreeMemory(device, memory, nullptr);
		vkDestroyBuffer(device, buffer, nullptr);
	}
};

struct BufferManager 
{
	GpuBuffer vertexBuffer;
	GpuBuffer indexBuffer;
	GpuBuffer meshletBuffer;

	void Cleanup(const Niagara::Device &device)
	{
		vertexBuffer.Destory(device);
		indexBuffer.Destory(device);
		meshletBuffer.Destory(device);
	}
};


/**
* What it takes to draw a triangle
* 1. Instance and physical device selection
* 2. Logical device and queue families
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
* * Draw frames by acquiring images, submitting the right draw command buffers and returning the images back to the swap chain
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
* Many structures in Vulkan require you to explicitly specify the type of structure in the `sType` member. The `pNext` member
* can point to an extension structure and will always be `nullptr` in this tutorial.
* Almost all functions return a `VkResult` that is either `VK_SUCCESS` or an error code.
*/

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo, const VkAllocationCallbacks * pAllocator, VkDebugUtilsMessengerEXT * pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

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

std::vector<const char*> GetInstanceExtensions()
{
#if 0
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#else
	std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Now it's only win32
#if defined(_WIN32)
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

#endif

	if (g_bEnableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// SRS - Dependency when VK_EXT_DEBUG_MARKER is enabled
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr; // Optional
}

VkDebugUtilsMessengerEXT SetupDebugMessenger(VkInstance instance)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	VkDebugUtilsMessengerEXT debugMessenger;
	VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger));

	return debugMessenger;
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
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	assert(!g_bEnableValidationLayers || CheckValidationLayerSupport(validationLayers));

	// Get extensions supported by the instance
	std::vector<std::string> supportedExtensions;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	if (extensionCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extensionCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) == VK_SUCCESS)
		{
			for (const auto &ext : extensions)
				supportedExtensions.push_back(ext.extensionName);
		}
	}

	std::vector<const char*> extensions = GetInstanceExtensions();

	if (!g_InstanceExtensions.empty())
	{
		for (const char *enabledExt : g_InstanceExtensions)
		{
			// Output message if requested extension is not available
			if (std::find(supportedExtensions.begin(), supportedExtensions.end(), enabledExt) == supportedExtensions.end())
			{
				std::cerr << "Enabled instance extension: " << enabledExt << " is not present at instance level.\n";
				continue;
			}
			extensions.push_back(enabledExt);
		}
	}

	VkInstance instance = VK_NULL_HANDLE;
	{
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (g_bEnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else 
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

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
* that originate from different queue families and each family of queues allows only a subset of commands. For example, there could
* be a queue family that only allows processing of compute commands or one that only allows memory transfer related commands.
*/
struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	// It's actually possible that the queue families supporting drawing commands and the ones supporting presentation do not overlap.
	// Therefore we have to take into account that there could be a distinct presentation queue.
	std::optional<uint32_t> presentFamily;

	bool IsComplete() const
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

bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(g_DeviceExtensions.begin(), g_DeviceExtensions.end());

	for (const auto &extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}
	
	return requiredExtensions.empty();
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
		bool extensionsSupported  = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			Niagara::SwapChainSupportDetails swapChainSupport = Niagara::QuerySwapChainSupport(device, surface);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}
		bValid = indices.IsComplete() && extensionsSupported && swapChainAdequate;
	}

#ifndef NDEBUG
	if (bValid) 
	{
		std::cout << "GPU: " << props.deviceName << " is valid.\n";
	}
#endif

	return bValid;
}

VkPhysicalDevice GetPhysicalDevice(VkPhysicalDevice *physicalDevices, uint32_t physicalDeviceCount, VkSurfaceKHR surface)
{
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		if (IsDeviceSuitable(physicalDevices[i], surface))
		{
			physicalDevice = physicalDevices[i];
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

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.ppEnabledExtensionNames = g_DeviceExtensions.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(g_DeviceExtensions.size());
	// The `enabledLayerCount` and `ppEnabledLayerNames` fields of `VkDeviceCreateInfo` are ignored by up-to-date implementations.

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS || USE_MESHLETS
	// Specifying used device features
	VkPhysicalDeviceFeatures2 deviceFeatures{};
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	void** pNext = &deviceFeatures.pNext;

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS 
	VkPhysicalDevice8BitStorageFeatures features8Bit{};
	features8Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
	features8Bit.storageBuffer8BitAccess = true;

	VkPhysicalDevice16BitStorageFeatures features16Bit{};
	features16Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
	features16Bit.storageBuffer16BitAccess = true;

	*pNext = &features16Bit;
	features16Bit.pNext = &features8Bit;
	pNext = &features8Bit.pNext;
#endif

#if USE_MESHLETS
	VkPhysicalDeviceMeshShaderFeaturesNV featureMeshShader{};
	featureMeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
	featureMeshShader.meshShader = true;

	*pNext = &featureMeshShader;
#endif	

	createInfo.pNext = &deviceFeatures;

#else
	// Specifying used device features
	VkPhysicalDeviceFeatures deviceFeatures{};
	// TODO: Is this needed ?
	// deviceFeatures.vertexPipelineStoresAndAtomics = true;

	createInfo.pEnabledFeatures = &deviceFeatures;
#endif

	VkDevice device = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

	return device;
}

void GetImageViews(VkDevice device, std::vector<VkImageView> &imageViews, const std::vector<VkImage> &images, VkFormat imageFormat)
{
	imageViews.resize(images.size());

	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = imageFormat;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	// The `subresourceRange` field describes what the image's purpose is and which part of the image should be accessed.
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;
	for (size_t i = 0; i < images.size(); ++i)
	{
		createInfo.image = images[i];
		
		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]));
	}
}

#pragma region Pipeline

VkRenderPass GetRenderPass(VkDevice device, VkFormat format)
{
	// Attachment description
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// The `loadOp` and `storeOp` determine what to do with the data in the attachment before rendering and after rendering.
	// * LOAD: Preserve the existing contents of the attachments
	// * CLEAR: Clear the values to a constant at the start
	// * DONT_CARE: Existing contents are undefined; we don't care about them
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Subpasses and attachment references
	// A single render pass can consist of multiple subpasses. Subpasses are subsequent rendering operations that depend on
	// the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another.
	// If you group these rendering operations into one render pass, then Vulkan is able to reorder the operations and conserve memory bandwidth
	// for possibly better performance.
	VkAttachmentReference colorAttachementRef{};
	colorAttachementRef.attachment = 0; // attachment index
	colorAttachementRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachementRef;

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &colorAttachment;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VK_CHECK(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass));

	return renderPass;
}

struct GraphicsPipelineDetails
{
	using Shader = Niagara::Shader;

	Shader vertShader;
	Shader taskShader;
	Shader meshShader;
	Shader fragShader;

	// Descriptors
	uint32_t resourceNum = 0;
	uint32_t resourceMask = 0;
	VkDescriptorType resourceTypes[32];

	uint32_t descriptorStart = 0, descriptorEnd = 0;
	Niagara::DescriptorInfo resourceInfos[32];
	VkWriteDescriptorSet resourceSets[32];

	VkRenderPass renderPass;

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	VkDescriptorUpdateTemplate descriptorUpdateTemplate;
	VkPipelineLayout layout;
	VkPipeline pipeline;

	std::vector<const Shader*> GetPipelineShaders() const
	{
		return { &vertShader, &taskShader, &meshShader, &fragShader };
	}

	void GatherResource()
	{
		auto shaders = GetPipelineShaders();

		resourceNum = 0;
		resourceMask = 0;
		for (const auto& shader : shaders)
		{
			if (shader->resourceMask)
			{
				for (uint32_t i = 0; i < 32; ++i)
				{
					if (shader->resourceMask & (1 << i))
					{
						if (resourceMask & (1 << i))
							assert(resourceTypes[i] == shader->resourceTypes[i]);
						else
						{
							resourceTypes[i] = shader->resourceTypes[i];
							resourceMask |= 1 << i;
						}
						++resourceNum;
					}
				}
			}
		}

		descriptorStart = descriptorEnd = 0;
		for (uint32_t i = 0; i < 32; ++i)
		{
			if (resourceMask & (1 << i))
			{
				if (descriptorEnd == 0) descriptorStart = i;
				descriptorEnd = i + 1;
			}
		}
	}

	std::vector<VkPipelineShaderStageCreateInfo> GetShaderStagesCreateInfo() const
	{
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo;

		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.pName = "main";

		auto shaders = GetPipelineShaders();
		for (const auto& shader : shaders)
		{
			if (shader->module != VK_NULL_HANDLE)
			{
				shaderStageCreateInfo.module = shader->module;
				shaderStageCreateInfo.stage = shader->stage;

				shaderStagesCreateInfo.push_back(shaderStageCreateInfo);
			}
		}

		return shaderStagesCreateInfo;
	}

	VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, bool pushDescriptorsSupported = true)
	{
		auto shaders = GetPipelineShaders();

		std::vector<VkDescriptorSetLayoutBinding> setBindings = Shader::GetSetBindings(shaders, resourceTypes, resourceMask);

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pBindings = setBindings.data();
		createInfo.bindingCount = static_cast<uint32_t>(setBindings.size());
		createInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;

		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayout));
		assert(setLayout);

		descriptorSetLayouts.push_back(setLayout);

		return setLayout;
	}

	VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, uint32_t setLayoutIndex = 0, bool pushDescriptorsSupported = true)
	{
		auto shaders = GetPipelineShaders();

		std::vector<VkDescriptorUpdateTemplateEntry> entries = Shader::GetUpdateTemplateEntries(shaders);

		VkDescriptorUpdateTemplateCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
		createInfo.pDescriptorUpdateEntries = entries.data();
		createInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entries.size());
		createInfo.templateType = pushDescriptorsSupported ? VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR : VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
		createInfo.descriptorSetLayout = pushDescriptorsSupported ? nullptr : descriptorSetLayouts[setLayoutIndex];
		createInfo.pipelineLayout = layout;
		createInfo.pipelineBindPoint = bindPoint;

		VkDescriptorUpdateTemplate updateTemplate = VK_NULL_HANDLE;
		VK_CHECK(vkCreateDescriptorUpdateTemplate(device, &createInfo, nullptr, &updateTemplate));
		assert(updateTemplate);

		descriptorUpdateTemplate = updateTemplate;

		return updateTemplate;
	}

	VkPipelineLayout CreatePipelineLayout(VkDevice device, bool pushDescriptorSupported = true)
	{
		CreateDescriptorSetLayout(device, pushDescriptorSupported);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.back();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // Optional

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		assert(pipelineLayout);

		layout = pipelineLayout;

		return pipelineLayout;
	}

	void SetDescriptor(uint32_t index, const Niagara::DescriptorInfo &info)
	{
		assert(resourceMask & (1 << index));
		// TODO:
		// assert(resourceTypes[index] == buffer.Type)
		resourceInfos[index] = info;
	}

	void SetDescriptor(uint32_t index, VkWriteDescriptorSet descriptor)
	{
		assert(resourceMask & (1 << index));

		resourceSets[index] = descriptor;
	}

	std::vector<Niagara::DescriptorInfo> GetDescriptorInfos() const
	{
		if (descriptorEnd - descriptorStart == 0) return {};

		std::vector<Niagara::DescriptorInfo> descriptorInfos(descriptorEnd - descriptorStart);
		for (uint32_t i = descriptorStart; i < descriptorEnd; ++i)
		{
			if (resourceMask & (1 << i))
				descriptorInfos[i] = resourceInfos[i];
		}

		return descriptorInfos;
	}

	std::vector<VkWriteDescriptorSet> GetDescriptorSets() const
	{
		std::vector<VkWriteDescriptorSet> descriptorSets;

		for (uint32_t i = descriptorStart; i < descriptorEnd; ++i)
		{
			if (resourceMask & (1 << i))
			{
				descriptorSets.push_back(resourceSets[i]);
			}
		}

		return descriptorSets;
	}

	void Cleanup(VkDevice device)
	{
		vertShader.Cleanup(device);
		taskShader.Cleanup(device);
		meshShader.Cleanup(device);
		fragShader.Cleanup(device);

		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyPipelineLayout(device, layout, nullptr);
		vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, nullptr);
		for (const auto &setLayout : descriptorSetLayouts)
			vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};

VkPipeline GetGraphicsPipeline(VkDevice device, GraphicsPipelineDetails &pipelineDetails, VkExtent2D viewportExtent, bool pushDescriptorSupported = true)
{
#if DRAW_MODE == DRAW_SIMPLE_MESH
#if USE_MESHLETS
	pipelineDetails.meshShader.Load(device, "./CompiledShaders/SimpleMesh.mesh.spv");
#else
	pipelineDetails.vertShader.Load(device, "./CompiledShaders/SimpleMesh.vert.spv");
#endif
	pipelineDetails.fragShader.Load(device, "./CompiledShaders/SimpleMesh.frag.spv");

#else
	pipelineDetails.vertShader.Load(device, "./CompiledShaders/SimpleTriangle.vert.spv");
	pipelineDetails.fragShader.Load(device, "./CompiledShaders/SimpleTriangle.frag.spv");
#endif

	pipelineDetails.GatherResource();

	// Pipeline shader stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo = pipelineDetails.GetShaderStagesCreateInfo();

	// Dynamic state
	// When opting for dynamic viewport(s) and scissor rectangle(s) you need to enable the respective dynamic states for the pipeline
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(g_DynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = g_DynamicStates.data();

	// Vertex input
	// Bindings: spacing between data and whether the data is per-vertex or per-instance
	// Attribute descriptions: type of the attributes passed to the vertex shader, which binding to load them from and at which offset 
	auto bindingDesc = Vertex::GetBindingDescription();
	auto vertexAttribDescs = Vertex::GetAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
#if VERTEX_INPUT_MODE != 1
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttribDescs.data();
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttribDescs.size());
#endif

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputCreateInfo{};
	inputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputCreateInfo.primitiveRestartEnable = VK_FALSE;

	// Viewports and scissors
	// While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which regions pixels will
	// actually be stored. Any pixels outside the scissor rectangles will be discarded by the rasterizer. They function like a filter
	// rather than a transformation.
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)viewportExtent.width;
	viewport.height = (float)viewportExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = viewportExtent;

	VkPipelineViewportStateCreateInfo viewportCreateInfo{};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerStateCreateInfo{};
	rasterizerStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerStateCreateInfo.lineWidth = 1.0f;
	rasterizerStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_BACK_BIT VK_CULL_MODE_NONE
#if FLIP_VIEWPORT
	rasterizerStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
#else
	rasterizerStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
#endif
	rasterizerStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizerStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizerStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizerStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	// Multisampling
	VkPipelineMultisampleStateCreateInfo multiSamplingCreateInfo{};
	multiSamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multiSamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multiSamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multiSamplingCreateInfo.minSampleShading = 1.0f; // Optional
	multiSamplingCreateInfo.pSampleMask = nullptr; // Optional
	multiSamplingCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
	multiSamplingCreateInfo.alphaToOneEnable = VK_FALSE; // Optional

	// Depth and stencil testing
	// TODO...
	
	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// Render pass
	auto renderPass = pipelineDetails.renderPass;

	// Pipeline layout
	VkPipelineLayout pipelineLayout = pipelineDetails.CreatePipelineLayout(device, pushDescriptorSupported);

	// Descriptor update template
	VkDescriptorUpdateTemplate descriptorUpdateTemplate = pipelineDetails.CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, pushDescriptorSupported);

	// ==>
	
	VkGraphicsPipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.pStages = shaderStagesCreateInfo.data();
	createInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfo.size());
	createInfo.pInputAssemblyState = &inputCreateInfo;
	createInfo.pVertexInputState = &vertexInputCreateInfo;
	createInfo.pViewportState = &viewportCreateInfo;
	createInfo.pRasterizationState = &rasterizerStateCreateInfo;
	createInfo.pMultisampleState = &multiSamplingCreateInfo;
	createInfo.pDepthStencilState = nullptr;
	createInfo.pColorBlendState = &colorBlending;
	createInfo.pDynamicState = &dynamicStateCreateInfo;

	createInfo.layout = pipelineLayout;
	createInfo.renderPass = renderPass;
	createInfo.subpass = 0; // index

	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = -1;

	VkPipeline graphicsPipeline;
	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &graphicsPipeline));

	return graphicsPipeline;
}

#pragma endregion

void GetFramebuffers(VkDevice device, std::vector<VkFramebuffer> &framebuffers, const std::vector<VkImageView> &swapChainImageViews, const VkExtent2D &size, VkRenderPass renderPass)
{
	framebuffers.resize(swapChainImageViews.size());

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = renderPass;
	createInfo.attachmentCount = 1;
	createInfo.width = size.width;
	createInfo.height = size.height;
	createInfo.layers = 1;
	
	for (size_t i = 0; i < swapChainImageViews.size(); ++i)
	{
		VkImageView attachments[] =
		{
			swapChainImageViews[i]
		};

		createInfo.pAttachments = attachments;

		VK_CHECK(vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffers[i]));
	}
}

void RecordCommandBuffer(VkCommandBuffer cmd, GraphicsPipelineDetails &pipelineDetails,
	const std::vector<VkFramebuffer> &framebuffers, const BufferManager &bufferMgr, uint32_t imageIndex, const Mesh &mesh, const VkExtent2D &viewportExtent)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

	// Starting a render pass
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = pipelineDetails.renderPass;
	renderPassInfo.framebuffer = framebuffers[imageIndex];

	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = viewportExtent;

	VkClearValue clearColor;
	clearColor.color = { 0.2f, 0.2f, 0.6f, 1.0f };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Basic drawing commands
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineDetails.pipeline);

	const auto& vb = bufferMgr.vertexBuffer;
	const auto& ib = bufferMgr.indexBuffer;

#if VERTEX_INPUT_MODE == 1
	pipelineDetails.SetDescriptor(0, Niagara::DescriptorInfo(vb.buffer, VkDeviceSize(0), VkDeviceSize(vb.size)));

#if USE_MESHLETS
	const auto& mb = bufferMgr.meshletBuffer;

	pipelineDetails.SetDescriptor(1, Niagara::DescriptorInfo(mb.buffer, VkDeviceSize(0), VkDeviceSize(mb.size)));

#else
	if (ib.size > 0)
		vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
#endif

#if 1
	auto descriptorInfos = pipelineDetails.GetDescriptorInfos();
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelineDetails.descriptorUpdateTemplate, pipelineDetails.layout, 0, descriptorInfos.data());
#else
	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineDetails.layout, 0, ARRAYSIZE(writeDescriptors), writeDescriptors);
#endif
	
#else
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, &offset);

	if (ib.size > 0)
		vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
#endif	

	VkViewport viewport{};
#if !FLIP_VIEWPORT
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(viewportExtend.width);
	viewport.height = static_cast<float>(viewportExtend.height);
#else
	// VK_CULL_MODE_NONE
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(viewportExtent.height);
	viewport.width = static_cast<float>(viewportExtent.width);
	viewport.height = -static_cast<float>(viewportExtent.height);
#endif
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = viewportExtent;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

#if DRAW_MODE == DRAW_SIMPLE_MESH

#if USE_MESHLETS
	uint32_t taskCount = static_cast<uint32_t>(mesh.meshlets.size());
	vkCmdDrawMeshTasksNV(cmd, taskCount, 0);
		
#else
	uint32_t vertexCount = static_cast<uint32_t>(vb.size / sizeof(Vertex));
	uint32_t indexCount = static_cast<uint32_t>(ib.size / sizeof(uint32_t));
	if (ib.size > 0)
		vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
	else
		vkCmdDraw(cmd, vertexCount, 1, 0, 0);
#endif

#else
	vkCmdDraw(cmd, 3, 1, 0, 0);
#endif

	// Finishing up
	vkCmdEndRenderPass(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));
}

struct SyncObjects
{
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;
	
	void Cleanup(VkDevice device)
	{
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);	
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);	
		vkDestroyFence(device, inFlightFence, nullptr);	
	}
};

VkSemaphore GetSemaphore(VkDevice device)
{
	VkSemaphoreCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore semaphore = VK_NULL_HANDLE;
	VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));

	return semaphore;
}

VkFence GetFence(VkDevice device, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT)
{
	VkFenceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// Create the fence in the signaled state, so the first call to `vkWaitForFences()` returns immediately since the fence is already signaled
	createInfo.flags = flags;

	VkFence fence = VK_NULL_HANDLE;
	VK_CHECK(vkCreateFence(device, &createInfo, nullptr, &fence));

	return fence;
}

SyncObjects GetSyncObjects(VkDevice device)
{
	SyncObjects syncObjects{};

	{
		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &syncObjects.imageAvailableSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &syncObjects.renderFinishedSemaphore));
	}

	{
		syncObjects.inFlightFence = GetFence(device);
	}

	return syncObjects;
}

struct FrameResources
{
	VkCommandBuffer commandBuffer;
	SyncObjects syncObjects;

	void Cleanup(VkDevice device)
	{
		syncObjects.Cleanup(device);
	}
};

void GetFrameResources(const Niagara::Device &device, std::vector<FrameResources> &frameResources)
{
	frameResources.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		frameResources[i].commandBuffer = g_CommandMgr.GetCommandBuffer(device);
		frameResources[i].syncObjects = GetSyncObjects(device);
	}
}

void CleanupSwapChain(VkDevice device, VkSwapchainKHR swapChain, std::vector<VkImageView> &swapChainImageViews, std::vector<VkFramebuffer> &framebuffers)
{
	for(auto &view : swapChainImageViews)
		vkDestroyImageView(device, view, nullptr);

	for (auto &framebuffer : framebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);

	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

VkQueryPool GetQueryPool(VkDevice device, uint32_t queryCount)
{
	VkQueryPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.queryCount = queryCount;
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	// createInfo.pipelineStatistics = VkQueryPipelineStatisticFlagBits::

	VkQueryPool queryPool;
	VK_CHECK(vkCreateQueryPool(device, &createInfo, nullptr, &queryPool));

	return queryPool;
}


/// Main

void Render(VkDevice device, SyncObjects &syncObjects, uint32_t imageIndex,
	VkCommandBuffer cmd, GraphicsPipelineDetails& pipelineDetails, VkQueue graphicsQueue,
	std::vector<VkFramebuffer> &framebuffers, const BufferManager &bufferMgr, const Mesh &mesh, const VkExtent2D &viewportExtend)
{
	// Recording the command buffer
	vkResetCommandBuffer(cmd, 0);
 	RecordCommandBuffer(cmd, pipelineDetails, framebuffers, bufferMgr, imageIndex, mesh, viewportExtend);

	// Submitting the command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { syncObjects.imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount  = ARRAYSIZE(waitSemaphores);
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	VkSemaphore signalSemaphores[] = { syncObjects.renderFinishedSemaphore };
	submitInfo.pSignalSemaphores = signalSemaphores;
	submitInfo.signalSemaphoreCount = ARRAYSIZE(signalSemaphores);

	VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, syncObjects.inFlightFence));
}


bool LoadObj(std::vector<Vertex> &vertices, const char* path)
{
	fastObjMesh* obj = fast_obj_read(path);

	if (obj == nullptr)
		return false;

	size_t vertexCount = 0, indexCount = 0;
	for (uint32_t i = 0; i < obj->face_count; ++i)
	{
		vertexCount += obj->face_vertices[i];
		indexCount += (obj->face_vertices[i] - 2) * 3; // 3 -> 3, 4 -> 6
	}

	// Currently, duplicated vertices, vertexCount == indexCount
	vertexCount = indexCount;

	vertices.resize(vertexCount);

	size_t vertexOffset = 0, indexOffset = 0;
	for (uint32_t i = 0; i < obj->face_count; ++i)
	{
		for (uint32_t j = 0; j < obj->face_vertices[i]; ++j)
		{
			fastObjIndex objIndex = obj->indices[indexOffset + j];

			// Triangulate polygon on the fly; offset - 3 is always the first polygon vertex
			if (j >= 3)
			{
				vertices[vertexOffset + 0] = vertices[vertexOffset - 3];
				vertices[vertexOffset + 1] = vertices[vertexOffset - 1];
				vertexOffset += 2;
			}

			Vertex& v = vertices[vertexOffset++];
			
#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
			// P
			v.p.x = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 0]);
			v.p.y = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 1]);
			v.p.z = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 2]);
			// N
			v.n.x = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 0] * 127.f + 127.5f);
			v.n.y = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 1] * 127.f + 127.5f);
			v.n.z = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 2] * 127.f + 127.5f);
			// UV
			v.uv.x = meshopt_quantizeHalf(obj->texcoords[objIndex.t * 2 + 0]);
			v.uv.y = meshopt_quantizeHalf(obj->texcoords[objIndex.t * 2 + 1]);
#else
			// P
			v.p.x = obj->positions[objIndex.p * 3 + 0];
			v.p.y = obj->positions[objIndex.p * 3 + 1];
			v.p.z = obj->positions[objIndex.p * 3 + 2];
			// N
			v.n.x = obj->normals[objIndex.n * 3 + 0];
			v.n.y = obj->normals[objIndex.n * 3 + 1];
			v.n.z = obj->normals[objIndex.n * 3 + 2];
			// UV
			v.uv.x = obj->texcoords[objIndex.t * 2 + 0];
			v.uv.y = obj->texcoords[objIndex.t * 2 + 1];
#endif
		}

		indexOffset += obj->face_vertices[i];
	}

	assert(vertexOffset == indexCount);

	fast_obj_destroy(obj);
	
	return true;
}

bool LoadMesh(Mesh& mesh, const char* path, bool bIndexless = false)
{
	std::vector<Vertex> triVertices;
	if (!LoadObj(triVertices, path))
		return false;

	size_t indexCount = triVertices.size();

	if (bIndexless)
	{
		mesh.vertices = triVertices;
#if 0
		mesh.indices.resize(indexCount);
		for (size_t i = 0; i < indexCount; ++i)
			mesh.indices[i] = i;
#endif
	}
	else
	{	
		std::vector<uint32_t> remap(indexCount);
		size_t vertexCount = meshopt_generateVertexRemap(remap.data(), nullptr, indexCount, triVertices.data(), indexCount, sizeof(Vertex));

		std::vector<Vertex> vertices(vertexCount);
		std::vector<uint32_t> indices(indexCount);

		meshopt_remapVertexBuffer(vertices.data(), triVertices.data(), indexCount, sizeof(Vertex), remap.data());
		meshopt_remapIndexBuffer(indices.data(), nullptr, indexCount, remap.data());

		meshopt_optimizeVertexCache(indices.data(), indices.data(), indexCount, vertexCount);
		meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indexCount, vertices.data(), vertexCount, sizeof(Vertex));

		mesh.vertices.insert(mesh.vertices.end(), vertices.begin(), vertices.end());
		mesh.indices.insert(mesh.indices.end(), indices.begin(), indices.end());
	}

	// TODO: optimize the mesh for more efficient GPU rendering
	return true;
}

void BuildMeshlets(Mesh& mesh)
{
	std::vector<Meshlet> meshlets;
	Meshlet meshlet{};
	std::vector<uint8_t> meshletVertices(mesh.vertices.size(), 0xFF);
	uint8_t indexOffset = 0, vertexOffset = 0;
	for (size_t i = 0; i < mesh.indices.size(); i += 3)
	{
		uint32_t i0 = mesh.indices[i];
		uint32_t i1 = mesh.indices[i+1];
		uint32_t i2 = mesh.indices[i+2];

		uint8_t &v0 = meshletVertices[i0];
		uint8_t &v1 = meshletVertices[i1];
		uint8_t &v2 = meshletVertices[i2];

		if (meshlet.vertexCount + (v0 == 0xFF) + (v1 == 0xFF) + (v2 == 0xFF) > MESHLET_MAX_VERTICES ||
			meshlet.triangleCount >= MESHLET_MAX_PRIMITIVES)
		{
			mesh.meshlets.push_back(meshlet);
			for (uint8_t mv = 0; mv < meshlet.vertexCount; ++mv)
				meshletVertices[meshlet.vertices[mv]] = 0xFF;
			meshlet = {};
		}

		if (v0 == 0xFF)
		{
			v0 = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = i0;
		}
		if (v1 == 0xFF)
		{
			v1 = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = i1;
		}
		if (v2 == 0xFF)
		{
			v2 = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = i2;
		}

		meshlet.indices[meshlet.triangleCount * 3 + 0] = v0;
		meshlet.indices[meshlet.triangleCount * 3 + 1] = v1;
		meshlet.indices[meshlet.triangleCount * 3 + 2] = v2;
		meshlet.triangleCount++;
	}

	if (meshlet.triangleCount > 0)
		mesh.meshlets.push_back(meshlet);
}

uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties &memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

void GetGpuBuffer(GpuBuffer& result, VkDevice device, const VkPhysicalDeviceMemoryProperties& memProperties, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags propertyFlags)
{
	VkBufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Optional

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(device, &createInfo, nullptr, &buffer));

	// The buffer has been created, but it doesn't actually have any memory assigned to it yet. The first step of allocating
	// memory for the buffer is to query its memory requirements.
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memProperties, memoryRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDeviceMemory memory;
	VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));

	VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

	void* data = nullptr;
	vkMapMemory(device, memory, 0, size, 0, &data);

	result.buffer = buffer;
	result.memory = memory;
	result.data = data;
	result.size = size;
}

void DestroyBuffer(VkDevice device, const GpuBuffer &buffer)
{
	vkFreeMemory(device, buffer.memory, nullptr);
	vkDestroyBuffer(device, buffer.buffer, nullptr);
}


int main()
{
	cout << "Hello, Vulkan!" << endl;

	// Window
	int rc = glfwInit();
	if (rc == GLFW_FALSE) 
	{
		cout << "GLFW init failed!" << endl;
		return -1;
	}

	// Because GLFW was originally designed to create an OpenGL context, we need to tell it to not create an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Niagara", nullptr, nullptr);
	g_Window = window;
	glfwSetWindowUserPointer(window, nullptr);
	glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);

	// Vulkan
	VK_CHECK(volkInitialize());

	VkInstance instance = GetVulkanInstance();
	assert(instance);

	volkLoadInstance(instance);

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	if (g_bEnableValidationLayers)
	{
		debugMessenger = SetupDebugMessenger(instance);
		assert(debugMessenger);
	}

	// Device features
	VkPhysicalDeviceFeatures physicalDeviceFeatures{};

	// Device extensions
	void* pNextChain = nullptr;
#if USE_DEVICE_8BIT_16BIT_EXTENSIONS || USE_MESHLETS
	void** pNext = &pNextChain;

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS 
	VkPhysicalDevice8BitStorageFeatures features8Bit{};
	features8Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
	features8Bit.storageBuffer8BitAccess = true;

	VkPhysicalDevice16BitStorageFeatures features16Bit{};
	features16Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
	features16Bit.storageBuffer16BitAccess = true;

	*pNext = &features16Bit;
	features16Bit.pNext = &features8Bit;
	pNext = &features8Bit.pNext;
#endif

#if USE_MESHLETS
	VkPhysicalDeviceMeshShaderFeaturesNV featureMeshShader{};
	featureMeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
	featureMeshShader.meshShader = true;

	*pNext = &featureMeshShader;
#endif

#endif

	Niagara::Device device{};
	device.Init(instance, physicalDeviceFeatures, g_DeviceExtensions, pNextChain);

	volkLoadDevice(device);

	Niagara::Swapchain swapchain{};
	swapchain.Init(instance, device, window);

	g_CommandMgr.Init(device);

	// Retrieving queue handles
	VkQueue graphicsQueue = g_CommandMgr.graphicsQueue;
	assert(graphicsQueue);

	// Need swap chain surface format
	VkRenderPass renderPass = GetRenderPass(device, swapchain.colorFormat);
	assert(renderPass);

	std::vector<VkFramebuffer> framebuffers;
	GetFramebuffers(device, framebuffers, swapchain.imageViews, swapchain.extent, renderPass);
	
	// Pipeline
	GraphicsPipelineDetails pipelineDetails{};
	pipelineDetails.renderPass = renderPass;
	VkPipeline graphicsPipeline = GetGraphicsPipeline(device, pipelineDetails, swapchain.extent);
	assert(graphicsPipeline);
	pipelineDetails.pipeline = graphicsPipeline;

#if 0
	Niagara::GraphicsPipeline np;
	np.meshShader.Load(device, "./CompiledShaders/SimpleMesh.mesh.spv");
	np.fragShader.Load(device, "./CompiledShaders/SimpleMesh.frag.spv");
	np.Init(device);
#endif

	// Command buffers

	// Creating the synchronization objects
	// We'll need one semaphore to signal that an image has been acquired from the swapchain and is ready for rendering, another one to signal that
	// rendering has finished and presentation can happen, and a fence to make sure only one frame is rendering at a time.
	SyncObjects syncObjects = GetSyncObjects(device);

	std::vector<FrameResources> frameResources;
	GetFrameResources(device, frameResources);

	VkQueryPool queryPool = GetQueryPool(device, 128);
	assert(queryPool);

	// Mesh
	Mesh mesh{};
	
	const std::string fileName{ "kitten.obj" };
	bool bLoaded = LoadMesh(mesh, std::string(g_ResourcePath + fileName).data());
	if (!bLoaded)
	{
		std::cout << "Load mesh failed: " << fileName << std::endl;
		return -1;
	}

#if USE_MESHLETS
	// Meshlets
	BuildMeshlets(mesh);
#endif

	VkMemoryPropertyFlags hostVisibleMemPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryPropertyFlags deviceLocalMemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	BufferManager bufferMgr{};
	GpuBuffer &vb = bufferMgr.vertexBuffer, &ib = bufferMgr.indexBuffer;
	size_t vbSize = mesh.vertices.size() * sizeof(Vertex);

#if 1
	vb.Init(device, vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, mesh.vertices.data());
	if (!mesh.indices.empty())
	{
		size_t ibSize = mesh.indices.size() * sizeof(uint32_t);
		ib.Init(device, ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, mesh.indices.data());
	}
#if USE_MESHLETS
	GpuBuffer& meshletBuffer = bufferMgr.meshletBuffer;
	size_t mbSize = mesh.meshlets.size() * sizeof(Meshlet);
	meshletBuffer.Init(device, mbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, mesh.meshlets.data());
#endif

#else
	GetGpuBuffer(vb, device, memProperties, vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memPropertyFlags); // g_BufferSize
	memcpy_s(vb.data, vbSize, mesh.vertices.data(), vbSize);
	if (!mesh.indices.empty())
	{
		size_t ibSize = mesh.indices.size() * sizeof(uint32_t);
		GetGpuBuffer(ib, device, memProperties, ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memPropertyFlags);
		memcpy_s(ib.data, ibSize, mesh.indices.data(), ibSize);
	}
#endif

	uint32_t currentFrame = 0;
	double currentFrameTime = glfwGetTime() * 1000.0;

	// SPACE

	// Main loop
	/**
	 * Outline of a frame
	 * * Wait for the previous frame to finish
	 * * Acquire an image from the swap chain
	 * * Record a command buffer which draws the scene onto that image
	 * * Submit the recorded command buffer
	 * * Present the swap chain image
	 */
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Get resources
		SyncObjects &currentSyncObjects = frameResources[currentFrame].syncObjects;
		VkCommandBuffer currentCommandBuffer = frameResources[currentFrame].commandBuffer;

		// Semaphores
		// A semaphores is used to add order between queue operations.
		// There happens to be 2 kinds of semaphores in Vulkan, binary and timeline.
		// A semaphores is either unsignaled or signaled. It begins life as unsignaled. The way we use a semaphore to order queue operations is
		// by providing the same semaphore as a `signal` semaphore in one queue operation and as a `wait` semaphore in another queue operation.

		// Fences
		// A fence has a similar purpose, in that it is used to synchronize execution, but it is for ordering the execution on the CPU.
		// Simply put, if the host needs to know when the GPU has finished something, we use a fence.

		// Fetch back buffer
		vkWaitForFences(device, 1, &currentSyncObjects.inFlightFence, VK_TRUE, UINT64_MAX);
		
		uint32_t imageIndex = 0;
		VkResult result = swapchain.AcquireNextImage(device, currentSyncObjects.imageAvailableSemaphore, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || g_FramebufferResized)
		{
			vkDeviceWaitIdle(device);

			// Recreate swapchain
			swapchain.UpdateSwapchain(device, window, false);

			// Recreate framebuffers
			for (auto &framebuffer : framebuffers)
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			GetFramebuffers(device, framebuffers, swapchain.imageViews, swapchain.extent, renderPass);

			// Recreate semaphores
			vkDestroySemaphore(device, currentSyncObjects.imageAvailableSemaphore, nullptr);
			currentSyncObjects.imageAvailableSemaphore = GetSemaphore(device);
			assert(currentSyncObjects.imageAvailableSemaphore);

			g_FramebufferResized = false;
			continue;
		}

		// Only reset the fence if we are submitting work
		vkResetFences(device, 1, &currentSyncObjects.inFlightFence);

		Render(device, currentSyncObjects, imageIndex,
			currentCommandBuffer, pipelineDetails, graphicsQueue, framebuffers, bufferMgr, mesh, swapchain.extent);

		// Present
		result = swapchain.QueuePresent(graphicsQueue, imageIndex, currentSyncObjects.renderFinishedSemaphore);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || g_FramebufferResized)
		{
			vkDeviceWaitIdle(device);

			swapchain.UpdateSwapchain(device, window, false);

			for (auto& framebuffer : framebuffers)
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			GetFramebuffers(device, framebuffers, swapchain.imageViews, swapchain.extent, renderPass);

			g_FramebufferResized = false;
			continue;
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		double frameEndTime = glfwGetTime() * 1000.0;
		double deltaTime = frameEndTime - currentFrameTime;
		currentFrameTime = frameEndTime;

		char title[256];
		sprintf_s(title, "Cpu %.1f ms, Gpu %.1f ms", deltaTime, 0.f);
		glfwSetWindowTitle(window, title);
	}

	// Wait for the logical device to finish operations before exiting main loop and destroying the window
	vkDeviceWaitIdle(device);

	glfwDestroyWindow(window);
	glfwTerminate();

	// SPACE

	// Clean up Vulkan

	// Resources

	vkDestroyQueryPool(device, queryPool, nullptr);

	for (auto &frameResource : frameResources)
	{
		frameResource.Cleanup(device);
	}
	
	syncObjects.Cleanup(device);

	for (auto &framebuffer : framebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	
	bufferMgr.Cleanup(device);

	// Pipeline
	pipelineDetails.Cleanup(device);

	g_CommandMgr.Cleanup(device);

	swapchain.Destroy(device);

	device.Destroy();
	
	if (g_bEnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
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
* are asynchronously executed by submitting them to a `VkQueue`. Queues are allocated from queue families, where each queue family
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
* allocated from a `VkCommandPool` that is associated with a specific queue family. To draw a simple triangle, we need to 
* record a command buffer with the following operations:
* * Begin the render pass
* * Bind the graphics pipeline
* * Draw 3 vertices
* * End the render pass
* 
* 8. Main loop
* Now that the drawing commands have been wrapped into a command buffer, the main loop is quite straightforward. We first acquire
* an image from the swap chain with `vkAcquireNextImageKHR`. We can then select the appropriate command buffer for that image and
* execute it with `vkQueueSubmit`. Finally, we return the image to the swap chain for presentation to the screen with `vkQueuePresentKHR`.
*/
