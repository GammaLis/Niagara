// Ref: Vulkan-Samples

#pragma once

#include "pch.h"
#include <unordered_set>


namespace Niagara
{
	class Device;
	class Image;

	class ImageView
	{
	public:
		ImageView() = default;

		void Init(const Device &device, Image& image, VkImageViewType viewType,
			uint32_t baseMipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t mipLevels = 1, uint32_t arrayLayers = 1);
		void Destroy(const Device &device);

		operator VkImageView() const { return view; }
		
		VkImageView view{ VK_NULL_HANDLE };
		VkImageSubresourceRange subresourceRange{};
		Image* image{ nullptr };
	};

	class Sampler
	{
	public:
		Sampler() = default;

		void Init(const Device &device, VkFilter filter = VK_FILTER_LINEAR, VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR, 
			VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, float maxAnisotropy = 0, 
			VkCompareOp compareOp = VK_COMPARE_OP_NEVER, VkSamplerReductionMode reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);
		void Destroy(const Device& device);

		operator VkSampler() const { return sampler; }

		VkSampler sampler{ VK_NULL_HANDLE };
	};

	class Image
	{
	public:
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		VkImageType type;
		VkExtent3D extent;
		VkFormat format;
		VkImageUsageFlags usage;
		VkSampleCountFlags sampleCount;
		VkImageTiling tiling;
		VkImageSubresource subresource;
		uint32_t arrayLayers = 0;
		std::vector<ImageView> views;

		uint8_t* mappedData{ nullptr };
		bool isMapped{ false };

		Image() = default;

		void Init(const Device &device, const VkExtent3D& extent, VkFormat format,
			VkImageUsageFlags imageUsage, VkImageCreateFlags flags, VkMemoryPropertyFlags memPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			uint32_t mipLevels = 1, uint32_t arrayLayers = 1,
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
			const uint32_t* queueFamilies = nullptr, uint32_t queueFamilyCount = 0);
		void Destroy(const Device &device);

		operator VkImage() const { return image; }

		const ImageView& CreateImageView(const Device &device, VkImageViewType viewType, uint32_t baseMipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t mipLevels = 1, uint32_t arrayLayers = 1);

		uint8_t* Map(const Device &device);
		void Unmap(const Device &device);
	};
}
