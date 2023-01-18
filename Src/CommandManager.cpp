#include "CommandManager.h"
#include "Device.h"

namespace Niagara
{
	// Globals variables

	CommandManager g_CommandMgr{};
	CommandContext g_CommandContext{};


	// global functions

	VkCommandBuffer BeginSingleTimeCommands()
	{
		VkCommandBuffer cmd = g_CommandMgr.CreateCommandBuffer(*g_Device, g_CommandMgr.commandPool);
		
		g_CommandContext.BeginCommandBuffer(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		return cmd;
	}

	void EndSingleTimeCommands(VkCommandBuffer cmd)
	{
		g_CommandContext.EndCommandBuffer(cmd);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.commandBufferCount = 1;

		vkQueueSubmit(g_CommandMgr.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(g_CommandMgr.graphicsQueue);

		vkFreeCommandBuffers(*g_Device, g_CommandMgr.commandPool, 1, &cmd);
	}

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags)
	{
		auto cmd = BeginSingleTimeCommands();

		// One of the most common ways to perform layout transitions is using an `image memory barrier`. A pipeline barrier
		// like that is generally used to sync access to resources, like ensuring that a write to a buffer completes before
		// reading from it, but it can also be used to transition image layouts and transfer queue family ownership when
		// `VK_SHARING_MODE_EXCLUSIVE` is used. There is an equivalent `buffer memory barrier` to do this for buffers.
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.subresourceRange = { aspectFlags, 0, 1, 0, 1 };
		// If you are using the barrier to transfer queue family ownership, then these two fields should be the indices of the queue families.
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		// Barriers are primarily used for sync purposes, so you must specify which types of operations that involve the resource must 
		// happen before the barrier, and which operations that involve the resource must wait on the barrier. We need to do that 
		// despite already using `vkQueueWaitIdle` to manually sync. The right values depend on the old and new layout.
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

		vkCmdPipelineBarrier(cmd, 0, 0, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		EndSingleTimeCommands(cmd);
	}

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags)
	{
		auto cmd = BeginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		// The `bufferRowLength` and `bufferImageHeight` fields specify how the pixels are laid out in memory. For example,
		// you could have some padding bytes between rows of the image. Specifying `0` for both indicates that the pixels 
		// are simply tightly pakced.
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = aspectFlags;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0 ,0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		EndSingleTimeCommands(cmd);
	}


	/// CommandManager

	VkCommandBuffer CommandManager::CreateCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = level;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

		return commandBuffer;
	}

	void CommandManager::Init(const Device& device)
	{
		commandPool = device.CreateCommandPool(device.queueFamilyIndices.graphics);

		vkGetDeviceQueue(device, device.queueFamilyIndices.graphics, 0, &graphicsQueue);
		vkGetDeviceQueue(device, device.queueFamilyIndices.compute, 0, &computeQueue);
		vkGetDeviceQueue(device, device.queueFamilyIndices.transfer, 0, &transferQueue);
	}

	void CommandManager::Cleanup(const Device& device)
	{
		vkDestroyCommandPool(device, commandPool, nullptr);
	}

	VkCommandBuffer CommandManager::GetCommandBuffer(const Device& device)
	{
		VkCommandBuffer cmd = CreateCommandBuffer(device, commandPool);
		return cmd;
	}


	/// CommandContext

	void CommandContext::UpdateDescriptorSetInfo(const Pipeline& pipeline)
	{
		for (uint32_t i = 0; i < Pipeline::s_MaxDescrptorSetNum; ++i)
			pipeline.UpdateDescriptorSetInfo(descriptorSetInfos[i], i);

		cachedPipelineLayout = pipeline.layout;

		cachedDescriptorSetLayouts = pipeline.descriptorSetLayouts;

		if (pipeline.descriptorUpdateTemplate)
			cachedDescriptorUpdateTemplate = pipeline.descriptorUpdateTemplate;
	}

	void CommandContext::BeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags usage)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = usage;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

		cachedCommandBuffer = cmd;
	}

	void CommandContext::BeginRenderPass(VkCommandBuffer cmd, VkRenderPass renderPass, VkFramebuffer framebuffer, const VkRect2D& renderArea, const std::vector<VkClearValue>& clearValues)
	{
		VkRenderPassBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginInfo.renderPass = renderPass;
		beginInfo.framebuffer = framebuffer;
		beginInfo.renderArea = renderArea;
		beginInfo.pClearValues = clearValues.data();
		beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

		vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

		cachedRenderPass = renderPass;
	}

	void CommandContext::BindPipeline(VkCommandBuffer cmd, const GraphicsPipeline& pipeline)
	{
		pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vkCmdBindPipeline(cmd, pipelineBindPoint, pipeline.pipeline);

		UpdateDescriptorSetInfo(pipeline);
	}

	void CommandContext::BindPipeline(VkCommandBuffer cmd, const ComputePipeline& pipeline)
	{
		pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
		vkCmdBindPipeline(cmd, pipelineBindPoint, pipeline.pipeline);

		UpdateDescriptorSetInfo(pipeline);
	}

	std::vector<DescriptorInfo> CommandContext::GetDescriptorInfos(uint32_t set) const
	{
		std::vector<DescriptorInfo> descriptorInfos;

		const auto& descriptorSetInfo = descriptorSetInfos[set];

		if (descriptorSetInfo.count == 0)
			return  descriptorInfos;

		descriptorInfos.resize(descriptorSetInfo.count);
		for (uint32_t i = descriptorSetInfo.start, imax = descriptorSetInfo.start + descriptorSetInfo.count; i < imax; ++i)
		{
			if (descriptorSetInfo.mask & (1 << i))
				descriptorInfos[i] = cachedDescriptorInfos[set][i];
		}

		return descriptorInfos;
	}

	std::vector<VkWriteDescriptorSet> CommandContext::GetDescriptorSets(uint32_t set) const
	{
		std::vector<VkWriteDescriptorSet> descriptorSets;

		const auto& descriptorSetInfo = descriptorSetInfos[set];

		for (uint32_t i = descriptorSetInfo.start, imax = descriptorSetInfo.start + descriptorSetInfo.count; i < imax; ++i)
		{
			if (descriptorSetInfo.mask & (1 << i))
			{
				descriptorSets.push_back(cachedWriteDescriptorSets[set][i]);
			}
		}

		return descriptorSets;
	}
}