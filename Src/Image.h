// Ref: Vulkan-Samples

#pragma once

#include "pch.h"
#include <unordered_set>


namespace Niagara
{
	class Image;

	class ImageView
	{
	public:
		ImageView() = default;

		void Init(VkDevice device, Image& image, VkImageViewType viewType,
			uint32_t baseMipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t mipLevels = 1, uint32_t arrayLayers = 1);
		void Destory(VkDevice device);
		
		VkImageView view = VK_NULL_HANDLE;
		VkImageSubresourceRange subresourceRange{};
		Image* image = nullptr;
	};

	class Image
	{
	public:
		VkImageView imageView = VK_NULL_HANDLE;
		VkImage image = VK_NULL_HANDLE;
		VkImageType type{};
		VkExtent3D extent{};
		VkFormat format{};
		VkImageUsageFlags usage{};
		VkSampleCountFlags sampleCount{};
		VkImageTiling tiling{};
		VkImageSubresource subresource{};
		uint32_t arrayLayers = 0;
		std::unordered_set<ImageView*> views;
		bool isMapped{ false };

		void Init(VkDevice device, const VkExtent3D& extent, VkFormat format,
			VkImageUsageFlags imageUsage, VkSampleCountFlagBits sampleCount, VkImageCreateFlags flags,
			uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkImageTiling tiling = VK_IMAGE_TILING_LINEAR,
			const uint32_t* queueFamilies = nullptr, uint32_t queueFamilyCount = 0);
		void Destroy(VkDevice);

		uint8_t* Map();
		void Unmap();
	};
}
