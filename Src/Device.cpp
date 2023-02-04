/*
* Vulkan device class
*
* Encapsulates a physical Vulkan device and its logical representation
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Device.h"
#include <iostream>

namespace Niagara
{
	/// Globals
	Device* g_Device = nullptr;

	bool g_PushDescriptorsSupported = true;


	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}


	std::vector<const char*> GetInstanceExtensions()
	{
		std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME };

		// Now it's only win32
#if defined(_WIN32)
		extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
		
		return extensions;
	}

	bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers)
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

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr; // Optional
	}

	// The very first thing you need to do is initialize the Vulkan library by creating an instance. 
	// The instance is the connection between your application and the Vulkan library and creating it 
	// involves specifying some details about your application to the driver.
	VkInstance GetVulkanInstance(const std::vector<const char*> &enabledExtensions, bool bEnableVadilationLayers)
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

		assert(!bEnableVadilationLayers || CheckValidationLayerSupport(validationLayers));

		// Get extensions supported by the instance
		std::vector<std::string> supportedExtensions;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		if (extensionCount > 0)
		{
			std::vector<VkExtensionProperties> extensions(extensionCount);
			if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) == VK_SUCCESS)
			{
				for (const auto& ext : extensions)
					supportedExtensions.push_back(ext.extensionName);
			}
		}

		std::vector<const char*> extensions = GetInstanceExtensions();
		if (bEnableVadilationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// SRS - Dependency when VK_EXT_DEBUG_MARKER is enabled
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		if (!enabledExtensions.empty())
		{
			for (const char* enabledExt : enabledExtensions)
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
			if (bEnableVadilationLayers)
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

	uint32_t GetGraphicsFamilyIndex(VkPhysicalDevice physicalDevice)
	{
		uint32_t queueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

		for (uint32_t i = 0; i < queueCount; ++i)
			if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				return i;

		return VK_QUEUE_FAMILY_IGNORED;
	}

	bool SupportsPresentation(VkPhysicalDevice physicalDevice, uint32_t familyIndex)
	{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		return !!vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, familyIndex);
#else
		return true;
#endif
	}

	VkPhysicalDevice CreatePhysicalDevice(VkInstance instance)
	{
		uint32_t physicalDeviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
		assert(physicalDeviceCount);

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()));

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties props{};

		for (uint32_t i = 0; i < physicalDeviceCount; ++i)
		{
			vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

			uint32_t graphicsFamilyIndex = GetGraphicsFamilyIndex(physicalDevices[i]);
			if (graphicsFamilyIndex == VK_QUEUE_FAMILY_IGNORED) continue;

			bool bSupportsPresentation = SupportsPresentation(physicalDevices[i], graphicsFamilyIndex);
			if (!bSupportsPresentation) continue;

			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = physicalDevices[i];
				break;
			}
		}

		if (physicalDevice)
			printf("Select GPU: %s.\n", props.deviceName);

		return physicalDevice;
	}


	// Device

	bool Device::Init(VkInstance instance, VkPhysicalDeviceFeatures enabledFeatures, const std::vector<const char*>& enabledExtensions, void* pNextChain, bool bUseSwapChain, VkQueueFlags requestedQueueTypes)
	{
		this->instance = instance;

		// FIXME...
		g_Device = this;

		physicalDevice = CreatePhysicalDevice(instance);
		if (!physicalDevice)
		{
			std::cerr << "Create physical device failed, nothing to do!\n";
			return false;
		}

		UpdatePhysicalDeviceProperties(physicalDevice);

		if (CreateLogicalDevice(enabledFeatures, enabledExtensions, pNextChain, bUseSwapChain, requestedQueueTypes) != VK_SUCCESS)
		{
			std::cerr << "Create logical device failed!\n";
			return false;
		}

		return true;
	}

	void Device::Destroy()
	{
		if (commandPool)
			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

		if (logicalDevice)
			vkDestroyDevice(logicalDevice, nullptr);

		g_Device = nullptr;
	}

	void Device::UpdatePhysicalDeviceProperties(VkPhysicalDevice physicalDevice)
	{
		assert(physicalDevice);
		this->physicalDevice = physicalDevice;

		// Store properties, features, limits and properties of the physical device for later use
		// Device properties also contain limits and sparse properties
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		// Features should be checked by the examples before using them
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		// Memory properties are used regularly for creating all kinds of buffers
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

		// Queue family properties, used for setting up requested queues upon device creation
		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		assert(queueFamilyCount > 0);
		queueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		// Get list of supported extensions
		supportedExtensions.clear();
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		if (extensionCount > 0)
		{
			std::vector<VkExtensionProperties> extensions(extensionCount);
			if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, &extensions.front()) == VK_SUCCESS)
			{
				for (const auto& ext : extensions)
					supportedExtensions.push_back(ext.extensionName);
			}
		}
	}

	// Get the index of a memory type that has all the requested property bits set
	uint32_t Device::GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound) const
	{
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
		{
			if ((typeBits & 1) == 1)
			{
				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					if (memTypeFound)
						*memTypeFound = true;

					return i;
				}
			}
			typeBits >>= 1;
		}

		if (memTypeFound)
		{
			*memTypeFound = false;
			return 0;
		}
		else 
			throw std::runtime_error("Could not find a matching memory type");
	}

	// Get the index of a queue family that supports the requested queue flags
	uint32_t Device::GetQueueFamilyIndex(VkQueueFlags queueFlags) const
	{
		uint32_t queueFamilyCount = static_cast<uint32_t>(queueFamilyProperties.size());

		// Dedicated queue for compute
		// Try to find a queue family index that supports compute but not graphics
		if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags)
		{
			for (uint32_t i = 0; i < queueFamilyCount; i++)
			{
				if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
				{
					return i;
				}
			}
		}

		// Dedicated queue for transfer
		if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags)
		{
			for (uint32_t i = 0; i < queueFamilyCount; i++)
			{
				if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && 
				   ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
				{
					return i;
				}
			}
		}

		// For other queue types of if no separate compute queue is present, return the first one to support the requested flags
		for (uint32_t i = 0; i < queueFamilyCount; i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
			{
				return i;
			}
		}

		throw std::runtime_error("Could not find a matching queue family index");
	}

	VkResult Device::CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, const std::vector<const char*>& enabledExtensions, void* pNextChain, bool bUseSwapChain, VkQueueFlags requestedQueueTypes)
	{
		// Desired queues need to be requested upon logical device creation
		// Due to differing queue family configuration of Vulkan implementation this can be a bit tricky, especially if the application
		// requests different queue types
		
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

		// Get queue family indices for the requested queue family types
		// Note that the indices may overlap depending on the implementation

		const float defaultQueuePriority{ 0.0f };

		// Graphics queue
		if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamilyIndices.graphics = GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);

			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
		else
		{
			queueFamilyIndices.graphics = 0;
		}

		// Dedicated compute queue
		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
		{
			queueFamilyIndices.compute = GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
			if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			// Else we use the same queue
			queueFamilyIndices.compute = queueFamilyIndices.graphics;
		}

		// Dedicated transfer queue
		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
		{
			queueFamilyIndices.transfer = GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
			if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
			{
				// If transfer family index differs, we need an additional queue create info for the transfer queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			// Else we use the same queue
			queueFamilyIndices.transfer = queueFamilyIndices.graphics;
		}

		// Create the logical device representation
#if 0
		std::vector<const char*> deviceExtensions(enabledExtensions);
#else
		std::vector<const char*> deviceExtensions;
		for (const char *ext : enabledExtensions)
		{
			if (IsExtensionSupported(ext))
				deviceExtensions.push_back(ext);
		}
#endif
		if (bUseSwapChain)
		{
			if (std::find(deviceExtensions.begin(), deviceExtensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME) == deviceExtensions.end())
			{
				// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
				deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			}
		}

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

		// If a pNext(Chain) has been passed, we need to add it to the device create info
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		if (pNextChain)
		{
			physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			physicalDeviceFeatures2.features = enabledFeatures;
			physicalDeviceFeatures2.pNext = pNextChain;
			deviceCreateInfo.pEnabledFeatures = nullptr;
			deviceCreateInfo.pNext = &physicalDeviceFeatures2;
		}

		// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
		if (IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
		{
			deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			enableDebugMarkers = true;
		}

		if (!deviceExtensions.empty())
		{
			for (const char* ext : deviceExtensions)
			{
				if (!IsExtensionSupported(ext))
					std::cerr << "Enabled device extension \"" << ext << "\" is not present at device level\n";
			}

			deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		}

		this->enabledFeatures = enabledFeatures;

		VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
		if (result != VK_SUCCESS)
			throw std::runtime_error("Can not create device");

		// Create a default command pool for graphics command buffers
		commandPool = CreateCommandPool(queueFamilyIndices.graphics);

		return result;
	}

	// Create a command pool for allocation command buffers from
	VkCommandPool Device::CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags) const
	{
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
		cmdPoolInfo.flags = createFlags;
		VkCommandPool cmdPool;
		VK_CHECK(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
		return cmdPool;
	}

	// Select the best-fit depth format for this device from a list of possible depth (and stencil) formats
	VkFormat Device::GetSupportedDepthFormat(bool bCheckSamplingSupport) const
	{
		// All depth formats may be optional, so we need to find a suitable depth format to use
		static std::vector<VkFormat> depthFormats
		{
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		for (auto& format : depthFormats)
		{
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
			// Format must support depth stencil attachment for optimal tiling
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				if (bCheckSamplingSupport)
					if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
						continue;
				return format;
			}
		}
		throw std::runtime_error("Could not find a matching depth format");
	}

	// Check if an extension is supported by the physical device
	bool Device::IsExtensionSupported(const std::string& extension) const
	{
		return std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end();
	}
}
