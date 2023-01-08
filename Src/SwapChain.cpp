#include "SwapChain.h"
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

	VkSwapchainKHR GetSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const SwapChainInfo& info)
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
}
