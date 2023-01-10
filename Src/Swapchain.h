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

	class Swapchain
	{
	private:
		VkInstance instance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;

	public:
		VkFormat colorFormat;
		VkExtent2D extent;
		VkColorSpaceKHR colorSpace;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		uint32_t imageCount = 0;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		uint32_t queueNodeIndex = UINT32_MAX;

		void Init(VkInstance instance, const Device& device, GLFWwindow* window);
		void Destroy(const Device& device);

		void InitSurface(VkInstance instance, const Device &device, GLFWwindow *window);
		void UpdateSwapchain(const Device &device, GLFWwindow *window, bool bVsync = true, bool bFullscreen = false);

		operator VkSwapchainKHR() const { return swapchain; }

		VkResult AcquireNextImage(const Device &device, VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
		VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

	private:
		void CreateImageViews(const Device &device);
	};
}
