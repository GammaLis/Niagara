// Ref: Vulkan-Samples - vk_common
#pragma once

#include "pch.h"


namespace Niagara
{
	// Determine if a Vulkan format is depth only.
	inline bool IsDepthOnlyFormat(VkFormat format)
	{
		return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
	}

	// Determine if a Vulkan format is depth or stencil
	inline bool IsDepthStencilFormat(VkFormat format)
	{
		return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
			IsDepthOnlyFormat(format);
	}

	// Determine if a Vulkan descriptor type is a dynamic storage buffer or dynamic uniform buffer.
	inline bool IsDynamicBufferDescriptorType(VkDescriptorType descriptorType)
	{
		return descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC || descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	}

	// Determine if a Vulkan descriptor type is a buffer (either uniform or storage buffer, dynmaic or not).
	inline bool IsBufferDescriptorType(VkDescriptorType descriptorType)
	{
		return descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			IsDynamicBufferDescriptorType(descriptorType);
	}

	uint32_t BitsPerPixel(VkFormat format);

	VkViewport GetViewport(const VkRect2D& rect, float minDepth = 0.0f, float maxDepth = 1.0f, bool flipViewport = false);

	// Image memory barrier structure used to define memory access for an image view during command recording
	struct ImageMemoryBarrier
	{
		VkPipelineStageFlags srcStateMask{ VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
		VkPipelineStageFlags dstStateMask{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		VkAccessFlags srcAccessMask{ 0 };
		VkAccessFlags dstAccessMask{ 0 };
		VkImageLayout oldLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		VkImageLayout newLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		uint32_t oldQueueFamily{ VK_QUEUE_FAMILY_IGNORED };
		uint32_t newQueueFamily{ VK_QUEUE_FAMILY_IGNORED };

		ImageMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask = 0, VkAccessFlags dstAccessMask = 0)
		{
			this->oldLayout = oldLayout;
			this->newLayout = newLayout;
			this->srcAccessMask = srcAccessMask;
			this->dstAccessMask = dstAccessMask;
		}
	};

	// Bufer memory barrier structure used to define memory access for a buffer during command recording
	struct BufferMemoryBarrier
	{
		VkPipelineStageFlags srcStateMask{ VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
		VkPipelineStageFlags dstStateMask{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		VkAccessFlags srcAccessMask{ 0 };
		VkAccessFlags dstAccessMask{ 0 };
	};

	VkImageMemoryBarrier GetImageMemoryBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT);

	// Put an image memory barrier for setting an image layout on the subresource into the given command buffer
	void SetImageLayout(VkCommandBuffer cmd, VkImage image, VkImageSubresourceRange subresourceRange,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		VkPipelineStageFlags srcMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	void SetImageLayout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectFlags,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		VkPipelineStageFlags srcMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);


	// Load and store info for a render pass attachment
	struct LoadStoreInfo 
	{
		VkAttachmentLoadOp loadOp{ VK_ATTACHMENT_LOAD_OP_CLEAR };
		VkAttachmentStoreOp storeOp{ VK_ATTACHMENT_STORE_OP_STORE };

		explicit LoadStoreInfo(VkAttachmentLoadOp load = VK_ATTACHMENT_LOAD_OP_CLEAR, VkAttachmentStoreOp store = VK_ATTACHMENT_STORE_OP_STORE)
			: loadOp{ load }, storeOp{ store }
		{  }
	};
}
