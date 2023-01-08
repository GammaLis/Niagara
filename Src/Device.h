/*
* Vulkan device class
*
* Encapsulates a physical Vulkan device and its logical representation
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once
#include "pch.h"


namespace Niagara
{
	class Device
	{
	public:
		// Physical device representation
		VkPhysicalDevice physicalDevice;
		// Logical device representation (application's view of the device)
		VkDevice logicalDevice;
		// Properties of the physical device including limits that the application can check against
		VkPhysicalDeviceProperties properties;
		// Features of the physical device that an application can use to check if a feature is supported
		VkPhysicalDeviceFeatures features;
		// Features that have been enabled for use on the physical device
		VkPhysicalDeviceFeatures enabledFeatures;
		// Memory types and heaps of the physical device
		VkPhysicalDeviceMemoryProperties memoryProperties;
		// Queue family properties of the physical device
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
		// List of extensions supported by the device
		std::vector<std::string> supportedExtensions;
		// Default command pool for the graphics queue family index
		VkCommandPool commandPool = VK_NULL_HANDLE;
		// Set to true when the debug marker extension is detected
		bool enableDebugMarkers = false;
		// Contains queue family indices
		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;

		explicit Device(VkPhysicalDevice physicalDevice);
		~Device();

		operator VkDevice() const { return logicalDevice; }

		uint32_t GetMeoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
		uint32_t GetQueueFamilyIndex(VkQueueFlags queueFlags) const;
		VkResult CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, const std::vector<const char*>& enabledExtensions, void* pNextChain, bool bUseSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
		VkCommandPool CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VkFormat GetSupportedDepthFormat(bool bCheckSamplingSupport);
		bool IsExtensionSupported(const std::string &extension);
	};
}
