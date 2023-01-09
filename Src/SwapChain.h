#pragma once
#include "pch.h"


struct GLFWwindow;

namespace Niagara
{
	class Device;

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct SwapChainInfo
	{
		VkSurfaceCapabilitiesKHR capabilities;
		VkSurfaceFormatKHR surfaceFormat;
		VkPresentModeKHR presentMode;
		VkExtent2D extent;
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
	SwapChainInfo GetSwapChainInfo(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, GLFWwindow* window);
	VkSwapchainKHR GetSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const SwapChainInfo& info);
	void GetSwapChainImages(VkDevice device, std::vector<VkImage>& swapChainImages, VkSwapchainKHR swapChain);

	class SwapChain
	{
	private:
		VkInstance instance;
		VkSurfaceKHR surface;

	public:
		VkFormat colorFormat;
		VkExtent2D extent;
		VkColorSpaceKHR colorSpace;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		uint32_t imageCount = 0;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		uint32_t queueNodeIndex = UINT32_MAX;

		void Init(VkInstance instance, const Device& device, void* platformHandle, void* platformWindow);
		void Destroy(const Device& device);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
		void InitSurface(VkInstance instance, const Device &device, void* platformHandle, void* platformWindow);
#endif
		void UpdateSwapChain(const Device &device, void *window, uint32_t& width, uint32_t& height, bool bVsync, bool bFullscreen);

		VkResult AcquireNextImage(const Device &device, VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
		VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

	private:
		void CreateImageViews(const Device &device);
	};
}
