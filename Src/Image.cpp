#include "Image.h"
#include "Device.h"
#include "VkCommon.h"
#include <iostream>


namespace Niagara
{
	/// ImageView

	void ImageView::Init(const Device &device, Image& image, VkImageViewType viewType, uint32_t baseMipLevel, uint32_t baseArrayLayer, uint32_t mipLevels, uint32_t arrayLayers)
	{
		Destroy(device);

		if (IsDepthStencilFormat(image.format))
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		else 
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = baseMipLevel;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.baseArrayLayer = baseArrayLayer;
		subresourceRange.layerCount = arrayLayers;

		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.format = image.format;
		createInfo.image = image.image;
		createInfo.subresourceRange = subresourceRange;
		createInfo.viewType = viewType;
		
		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &view));
		assert(view);

		this->image = &image;
	}

	void ImageView::Destroy(const Device &device)
	{
		if (view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, view, nullptr);
			view = VK_NULL_HANDLE;
		}
	}


	/// Sampler

	void Sampler::Init(const Device& device, VkFilter filter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode, float maxAnisotropy, VkCompareOp compareOp, VkSamplerReductionMode reductionMode)
	{
		Destroy(device);

		VkSamplerCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		createInfo.magFilter = filter;
		createInfo.minFilter = filter;
		createInfo.mipmapMode = mipmapMode;
		createInfo.addressModeU = addressMode;
		createInfo.addressModeV = addressMode;
		createInfo.addressModeW = addressMode;
		createInfo.mipLodBias = 0;
		createInfo.anisotropyEnable = maxAnisotropy > 0 ? VK_TRUE : VK_FALSE;
		createInfo.maxAnisotropy = maxAnisotropy;
		createInfo.compareEnable = compareOp != VK_COMPARE_OP_NEVER ? VK_TRUE : VK_FALSE;
		createInfo.compareOp = compareOp;
		createInfo.minLod = 0.0f;
		createInfo.maxLod = 16.0f;

		// Reduction mode, eg. min depth pyramid
		VkSamplerReductionModeCreateInfo reductionModeCreateInfo{};
		if (reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE)
		{
			reductionModeCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
			reductionModeCreateInfo.reductionMode = reductionMode;
			createInfo.pNext = &reductionModeCreateInfo;
		}
		
		VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));
		assert(sampler);
	}

	void Sampler::Destroy(const Device& device)
	{
		if (sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, sampler, nullptr);
			sampler = VK_NULL_HANDLE;
		}
	}


	/// Image

	static VkImageType GetImageType(VkExtent3D extent)
	{
		VkImageType type{};

		uint32_t dimNum = 0;

		if (extent.width > 1)
			dimNum++;
		if (extent.height > 1)
			dimNum++;
		if (extent.depth > 1)
			dimNum++;

		switch (dimNum)
		{
		case 1:
			type = VK_IMAGE_TYPE_1D;
			break;
		case 2:
			type = VK_IMAGE_TYPE_2D;
			break;
		case 3:
			type = VK_IMAGE_TYPE_3D;
			break;
		default:
			throw std::runtime_error("No image type found.");
			break;
		}

		return type;
	}

	static VkImageViewType GetImageViewType(VkImageType imageType, uint32_t arrayLayers = 1, bool cubeImage = false)
	{
		switch (imageType)
		{
		case VK_IMAGE_TYPE_1D:
			return arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:
			return arrayLayers > 1 ? 
				cubeImage && arrayLayers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D_ARRAY : 
				VK_IMAGE_VIEW_TYPE_2D;
		case VK_IMAGE_TYPE_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		default:
			throw std::runtime_error("No image view type found.");
			return VK_IMAGE_VIEW_TYPE_2D;
		}
	}

	/**
	* Tiling
	* * VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order 
	* * VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
	*/
	void Image::Init(const Device &device, const VkExtent3D& extent, VkFormat format, 
		VkImageUsageFlags imageUsage, VkImageCreateFlags flags, VkMemoryPropertyFlags memPropertyFlags,
		uint32_t mipLevels, uint32_t arrayLayers,
		VkSampleCountFlagBits sampleCount, VkImageTiling tiling,
		const uint32_t* queueFamilies, uint32_t queueFamilyCount)
	{
		Destroy(device);

		this->type = GetImageType(extent);
		this->extent = extent;
		this->format = format;
		this->sampleCount = sampleCount;
		this->usage = imageUsage;
		this->arrayLayers = arrayLayers;
		this->tiling = tiling;

		subresource.arrayLayer = arrayLayers;
		subresource.mipLevel = mipLevels;
		subresource.aspectMask = IsDepthStencilFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createInfo.flags = flags;
		createInfo.imageType = type;
		createInfo.format = format;
		createInfo.extent = extent;
		createInfo.mipLevels = mipLevels;
		createInfo.arrayLayers = arrayLayers;
		createInfo.samples = sampleCount;
		createInfo.tiling = tiling;
		createInfo.usage = imageUsage;

		if (queueFamilies != nullptr)
		{
			createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = queueFamilyCount;
			createInfo.pQueueFamilyIndices = queueFamilies;
		}

		VK_CHECK(vkCreateImage(device, &createInfo, nullptr, &image));
		assert(image);

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
			memPropertyFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		VkBool32 memTypeFound = VK_FALSE;
		allocInfo.memoryTypeIndex = device.GetMemoryType(memRequirements.memoryTypeBits, memPropertyFlags, &memTypeFound);
		assert(memTypeFound);

		VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));
		assert(memory);

		vkBindImageMemory(device, image, memory, 0);

		// Create default view
		CreateImageView(device, GetImageViewType(type, arrayLayers), 0, 0, mipLevels, arrayLayers);
	}

	void Image::Destroy(const Device& device)
	{
		if (!views.empty())
		{
			for (auto& view : views)
				view.Destroy(device);

			views.clear();
		}

		if (memory != VK_NULL_HANDLE)
		{
			// Unmap(device);
			vkFreeMemory(device, memory, nullptr);
		}
		if (image != VK_NULL_HANDLE)
			vkDestroyImage(device, image, nullptr);
	}

	const ImageView& Image::CreateImageView(const Device &device, VkImageViewType viewType, uint32_t baseMipLevel, uint32_t baseArrayLayer, uint32_t mipLevels, uint32_t arrayLayers)
	{
		views.push_back({});

		auto &view = views.back();
		view.Init(device, *this, viewType, baseMipLevel, baseArrayLayer, mipLevels > 0 ? mipLevels : subresource.mipLevel, arrayLayers > 0 ? arrayLayers : subresource.arrayLayer);

		return view;
	}

	uint8_t* Image::Map(const Device &device)
	{
		if (!mappedData)
		{
			if (tiling != VK_IMAGE_TILING_LINEAR)
				std::cerr << "Mapping image memory that is not linear.\n";
			
			// vkMapMemory(device, memory, VkDeviceSize(0), )
			isMapped = true;
		}

		return mappedData;
	}

	void Image::Unmap(const Device &device)
	{
		vkUnmapMemory(device, memory);
		mappedData = nullptr;
		isMapped = false;
	}
}
