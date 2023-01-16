#include "Image.h"
#include "Utilities.h"


namespace Niagara
{
	/// ImageView

	void ImageView::Init(VkDevice device, Image& image, VkImageViewType viewType, uint32_t baseMipLevel, uint32_t baseArrayLayer, uint32_t mipLevels, uint32_t arrayLayers)
	{
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

	void ImageView::Destory(VkDevice device)
	{
		if (view != VK_NULL_HANDLE)
			vkDestroyImageView(device, view, nullptr);
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

	void Image::Init(VkDevice device, const VkExtent3D& extent, VkFormat format, 
		VkImageUsageFlags imageUsage, VkSampleCountFlagBits sampleCount, VkImageCreateFlags flags, 
		uint32_t mipLevels, uint32_t arrayLayers, VkImageTiling tiling,
		const uint32_t* queueFamilies, uint32_t queueFamilyCount)
	{
		this->type = GetImageType(extent);
		this->format = format;
		this->sampleCount = sampleCount;
		this->usage = usage;
		this->arrayLayers = arrayLayers;
		this->tiling = tiling;

		subresource.arrayLayer = arrayLayers;
		subresource.mipLevel = mipLevels;

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
		createInfo.usage = usage;

		if (queueFamilies != nullptr)
		{
			createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = queueFamilyCount;
			createInfo.pQueueFamilyIndices = queueFamilies;
		}

		VK_CHECK(vkCreateImage(device, &createInfo, nullptr, &image));
		assert(image);
	}
}
