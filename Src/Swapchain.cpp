/*
* Class wrapping access to the swap chain
*
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "Swapchain.h"
#include "Device.h"

#include <GLFW/glfw3.h>

namespace Niagara
{
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		// Each `VkSurfaceFormatKHR` entry contains a `format` and a `colorSpace` member.
		for (const auto& surfaceFormat : availableFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return surfaceFormat;
		}

		// Fallback
		return availableFormats[0];
	}

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for (const auto& presentMode : availablePresentModes)
		{
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return presentMode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
	{
		// The swap extent is the resolution of the swap chain images and it's almost always exactly equal to the resolution of the window that
		// we're drawing to `in pixels`.
		// GLFW uses 2 units when measuring sizes: pixels and screen coordinates. For example, the resolution {WIDTH, HEIGHT} is measured in screen coordinates.
		// If you are using a high DPI display, screen coordinates don't correspond to pixels. Due to the higher pixel density, the resolution of the window
		// in pixels will be larger than the resolution in screen coordinates.
		// We must use `glfwGetFramebufferSize` to query the resolution of the window in pixel before matching it against the minimum and maximum image extent.
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent =
			{
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	SwapChainInfo GetSwapChainInfo(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, GLFWwindow* window)
	{
		SwapChainSupportDetails swapChainDetails = QuerySwapChainSupport(physicalDevice, surface);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainDetails.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainDetails.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainDetails.capabilities, window);

		// Handling minimization
		{
			while (extent.width == 0 || extent.height == 0)
			{
				glfwWaitEvents();
				swapChainDetails = QuerySwapChainSupport(physicalDevice, surface);
				extent = ChooseSwapExtent(swapChainDetails.capabilities, window);
			}
		}

		SwapChainInfo info{};
		info.capabilities = swapChainDetails.capabilities;
		info.surfaceFormat = surfaceFormat;
		info.presentMode = presentMode;
		info.extent = extent;
		return info;
	}

	VkSwapchainKHR GetSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const SwapChainInfo& info)
	{
		VkSurfaceFormatKHR surfaceFormat = info.surfaceFormat;
		VkPresentModeKHR presentMode = info.presentMode;
		VkExtent2D extent = info.extent;

		// It is recommended to request at least one more image than the minimum
		uint32_t imageCount = info.capabilities.minImageCount + 1;
		if (info.capabilities.maxImageCount > 0 && imageCount > info.capabilities.maxImageCount)
			imageCount = info.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		// The `imageArrayLayers` specifies the amount of layers each image consists of. This is always 1 unless you are developing a stereoscopic 3D application.
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

#if 0
		// We need to specify how to handle swap chain images that will be used across multiple queue families. That will be the case in our app
		// if the graphics queue family is different from the presentation family. We'll be drawing on the images in the swap chain from the graphics queue
		// and then submitting them on the presentation queue.
		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily)
		{
			// Images can be used across multiple queue families without explicit ownership transfers.
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
#endif
		{
			// An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family.
			// This option offers the best performance
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // optional
			createInfo.pQueueFamilyIndices = nullptr; // optional
		}

		// We can specify that a certain transform should be applied to images in the swap chain if it is supported, like a 90 degree clockwise rotation or horizontal flip.
		// To specify that you do not want any transformation, simply specify the current transformation.
		createInfo.preTransform = info.capabilities.currentTransform;
		// The `compositeAlpha` field specifies if the alpha channel should be used for blending with other windows in the window system.
		// You'll almost always want to simply ignore the alpha channel.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkSwapchainKHR swapChain;
		VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain));

		return swapChain;
	}

	void GetSwapChainImages(VkDevice device, std::vector<VkImage>& swapChainImages, VkSwapchainKHR swapChain)
	{
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
	}


	// SwapChain
	void Swapchain::Init(VkInstance instance, const Device& device, GLFWwindow* window)
	{
		this->instance = instance;

		InitSurface(instance, device, window);

		UpdateSwapchain(device, window, false);
	}

	void Swapchain::InitSurface(VkInstance instance, const Device &device, GLFWwindow *window)
	{
		// Create the os-specific surface
#if 0
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hwnd = (HWND)glfwGetWin32Window(window);
		surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface));
#else
		VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
#endif

		// Default graphics queue supports presentation
		queueNodeIndex = device.queueFamilyIndices.graphics;

		// Get list of supported surface formats
		uint32_t formatCount;
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, nullptr));
		assert(formatCount > 0);
		
		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, surfaceFormats.data()));

		// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
		// there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
		const VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_SRGB; // VK_FORMAT_B8G8R8A8_SRGB VK_FORMAT_B8G8R8A8_UNORM
		const VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
		{
			colorFormat = preferredFormat;
			colorSpace = surfaceFormats[0].colorSpace;
		}
		else
		{
			// Iterate over the list of available surface format and 
			// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
			bool bFoundPreferred = false;
			for (auto&& surfaceFormat : surfaceFormats)
			{
				if (surfaceFormat.format == preferredFormat)
				{
					colorFormat = surfaceFormat.format;
					colorSpace = surfaceFormat.colorSpace;
					bFoundPreferred = true;
					break;
				}
			}

			// in case VK_FORMAT_B8G8R8A8_UNORM is not available, select the first available color format
			if (!bFoundPreferred)
			{
				colorFormat = surfaceFormats[0].format;
				colorSpace = surfaceFormats[0].colorSpace;
			}
		}
	}

	// Update the swap chain and get its images with given width and height
	void Swapchain::UpdateSwapchain(const Device &device, GLFWwindow *window, bool bVsync, bool bFullscreen)
	{
		auto physicalDevice = device.physicalDevice;

		// Store the current swapchain handle so we can use it later on to ease up recreation
		VkSwapchainKHR oldSwapchain = swapchain;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surfCaps;
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps));

		VkExtent2D swapchainExtent = ChooseSwapExtent(surfCaps, window);
		// Handling minimization
		while (swapchainExtent.width == 0 || swapchainExtent.height == 0)
		{
			glfwWaitEvents();

			VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps));
			swapchainExtent = ChooseSwapExtent(surfCaps, window);
		}
		extent = swapchainExtent;

		// Present mode

		// Get available present modes
		uint32_t presentModeCount;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
		assert(presentModeCount > 0);

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

		// Select a present mode for the swapchain

		// The VK_PRESENT_MODE_FIFO_KHR mode must always to present as per spec
		// This mode waits for teh vertical blank ("v-sync")
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!bVsync)
		{
			for (uint32_t i = 0; i < presentModeCount; ++i)
			{
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
					presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}

		// Determine the number of images
		uint32_t swapchainImageCount = surfCaps.minImageCount + 1;
		if ((surfCaps.maxImageCount > 0) && (swapchainImageCount > surfCaps.maxImageCount))
			swapchainImageCount = surfCaps.maxImageCount;

		// Find the transformation of the surface
		VkSurfaceTransformFlagsKHR preTransform;
		if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			// We prefer a non-rotated transform
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else
		{
			preTransform = surfCaps.currentTransform;
		}

		// Find a supported composite alpha format (not all devices support alpha opaque)
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		// Simply select the first composite alpha format available
		const std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = 
		{
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (const auto& compositeAlphaFlag : compositeAlphaFlags)
		{
			if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
			{
				compositeAlpha = compositeAlphaFlag;
				break;
			}
		}

		// ==> Create
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = swapchainImageCount;
		createInfo.imageFormat = colorFormat;
		createInfo.imageColorSpace = colorSpace;
		createInfo.imageExtent = { swapchainExtent.width, swapchainExtent.height };
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
		createInfo.imageArrayLayers = 1;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.presentMode = presentMode;
		// Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
		createInfo.oldSwapchain = oldSwapchain;
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		createInfo.clipped = VK_TRUE;
		createInfo.compositeAlpha = compositeAlpha;

		// Enable transfer source on swapchain images if supported
		if (surfCaps.supportedUsageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
			createInfo.imageUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		// Enable transfer destination on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain));

		// If an existing swapchain is re-created, destroy the old swap chain
		// This can also cleans up all the presentable images
		if (oldSwapchain != VK_NULL_HANDLE)
		{
			for (uint32_t i = 0; i < imageCount; ++i)
				vkDestroyImageView(device, imageViews[i], nullptr);

			vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
		}

		// Get the swapchain images
		VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
		assert(imageCount > 0);

		images.resize(imageCount);
		VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()));

		CreateImageViews(device);
	}

	// Acquire the next image in the swapchain
	VkResult Swapchain::AcquireNextImage(const Device &device, VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
	{
		// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
		// With that we don't have to handle VK_NOT_READY
		return vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentCompleteSemaphore, VK_NULL_HANDLE, imageIndex);
	}

	// Queue an image for presentation
	VkResult Swapchain::QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.swapchainCount = 1;
		presentInfo.pImageIndices = &imageIndex;
		// Check if a wait semaphore has been specified to wait for before presenting the image
		if (waitSemaphore != VK_NULL_HANDLE)
		{
			presentInfo.pWaitSemaphores = &waitSemaphore;
			presentInfo.waitSemaphoreCount = 1;
		}
		
		return vkQueuePresentKHR(queue, &presentInfo);
	}

	// Destroy and free Vulkan resources used for the swapchain
	void Swapchain::Destroy(const Device &device)
	{
		if (swapchain != VK_NULL_HANDLE)
		{
			for (uint32_t i = 0; i < imageCount; ++i)
				vkDestroyImageView(device, imageViews[i], nullptr);
			
			vkDestroySwapchainKHR(device, swapchain, nullptr);
		}
		if (surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(instance, surface, nullptr);
		}
	}

	void Swapchain::CreateImageViews(const Device& device)
	{
		imageViews.resize(imageCount);
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = colorFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		// The `subresourceRange` field describes what the image's purpose is and which part of the image should be accessed.
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		for (size_t i = 0; i < images.size(); ++i)
		{
			imageViewCreateInfo.image = images[i];
			VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]));
		}
	}
}
