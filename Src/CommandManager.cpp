#include "CommandManager.h"
#include "Device.h"
#include "Image.h"
#include "Renderer.h"

namespace Niagara
{
	// Globals variables

	CommandManager g_CommandMgr{};
	CommandContext g_CommandContext{};


	// Global functions

	VkCommandBuffer BeginSingleTimeCommands(EQueueFamily queueFamily)
	{
		VkCommandBuffer cmd = g_CommandMgr.CreateCommandBuffer(*g_Device, queueFamily);		
		g_CommandContext.BeginCommandBuffer(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		return cmd;
	}

	void EndSingleTimeCommands(VkCommandBuffer cmd, EQueueFamily queueFamily)
	{
		g_CommandContext.EndCommandBuffer(cmd);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.commandBufferCount = 1;

		VkQueue queue = g_CommandMgr.GetCommandQueue(queueFamily);
		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(*g_Device, g_CommandMgr.GetCommandPool(queueFamily), 1, &cmd);
	}

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags,
		VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask)
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

		vkCmdPipelineBarrier(cmd, srcMask, dstMask, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		EndSingleTimeCommands(cmd);
	}

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags, EQueueFamily queueFamily)
	{
		auto cmd = BeginSingleTimeCommands(queueFamily);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		// The `bufferRowLength` and `bufferImageHeight` fields specify how the pixels are laid out in memory. For example,
		// you could have some padding bytes between rows of the image. Specifying `0` for both indicates that the pixels 
		// are simply tightly packed.
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = aspectFlags;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0 ,0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		EndSingleTimeCommands(cmd, queueFamily);
	}

	void InitializeTexture(const Device &device, Texture& texture, const void *pInitData, size_t size)
	{
		Buffer stagingBuffer;
		stagingBuffer.Init(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 
			VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, pInitData, size);

		VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		{
			ScopedCommandBuffer cmd(device, EQueueFamily::Transfer);

			VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			// Transition
			{
				g_CommandContext.ImageBarrier2(texture.image, VkImageSubresourceRange{aspectMask, 0, 1, 0, 1} ,
					VK_IMAGE_LAYOUT_UNDEFINED, dstLayout,
					VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT);
				g_CommandContext.PipelineBarriers2(cmd);
			}

			// Copy
			{
				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				// The `bufferRowLength` and `bufferImageHeight` fields specify how the pixels are laid out in memory. For example,
				// you could have some padding bytes between rows of the image. Specifying `0` for both indicates that the pixels 
				// are simply tightly packed.
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = aspectMask;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageOffset = { 0 ,0, 0 };
				region.imageExtent = texture.extent;

				vkCmdCopyBufferToImage(cmd, stagingBuffer, texture.image, dstLayout, 1, &region);
			}

			// Transition
			{
				g_CommandContext.ImageBarrier2(texture.image, VkImageSubresourceRange{ aspectMask, 0, 1, 0, 1 },
					dstLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
				g_CommandContext.PipelineBarriers2(cmd);
			}
		}

		stagingBuffer.Destroy(device);
	}

	
	/// CommandPool

	void CommandPool::Init(const Device& device, uint32_t familyIndex, VkCommandPoolCreateFlags flags, VkQueue queue)
	{
		m_Device = device;

		VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		createInfo.queueFamilyIndex = familyIndex;
		createInfo.flags = flags;
		vkCreateCommandPool(device, &createInfo, nullptr, &m_CommandPool);
		
		if (queue)
			m_CommandQueue = queue;
		else
			vkGetDeviceQueue(device, familyIndex, 0, &m_CommandQueue);
	}

	void CommandPool::Destroy()
	{
		if (m_CommandPool)
		{
			vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
			m_CommandPool = VK_NULL_HANDLE;
		}

		m_Device = VK_NULL_HANDLE;
	}

	VkCommandBuffer CommandPool::CreateCommandBuffer(VkCommandBufferLevel level)
	{
		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = m_CommandPool;
		allocInfo.level = level;

		VkCommandBuffer cmd;
		vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);

		return cmd;
	}

	void CommandPool::Free(uint32_t count, const VkCommandBuffer* cmds)
	{
		vkFreeCommandBuffers(m_Device, m_CommandPool, count, cmds);
	}

	VkCommandBuffer CommandPool::GetCommandBuffer(uint64_t fenceVal, VkCommandBufferLevel level)
	{
		if (!m_UsedCmds.empty())
		{
			auto frontPair = m_UsedCmds.front();
			while (!m_UsedCmds.empty() && frontPair.first + Renderer::MAX_FRAMES_IN_FLIGHT <= fenceVal)
			{
				m_UsedCmds.pop();
				// vkResetCommandBuffer(frontPair.second, 0);
				m_FreeCmds.push(frontPair.second);
				
				frontPair = m_UsedCmds.front();
			}
		}

		VkCommandBuffer cmd{ VK_NULL_HANDLE };
		if (!m_FreeCmds.empty())
		{
			cmd = m_FreeCmds.front();
			m_FreeCmds.pop();
		}
		else
		{
			cmd = CreateCommandBuffer(level);
		}
		vkResetCommandBuffer(cmd, 0);
		m_UsedCmds.push({ fenceVal, cmd });

		return cmd;
	}

	void CommandPool::Submit(uint32_t count, const VkCommandBuffer* cmds, VkFence fence, 
		uint32_t waitSemaphoreCount, VkSemaphore* waitSemaphores, VkPipelineStageFlags *waitStages,
		uint32_t signalSemaphoreCount, VkSemaphore* signalSemaphores)
	{
		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = count;
		submitInfo.pCommandBuffers = cmds;
		if (waitSemaphoreCount > 0 && waitSemaphores)
		{
			submitInfo.waitSemaphoreCount = waitSemaphoreCount;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}
		if (signalSemaphoreCount > 0 && signalSemaphores)
		{
			submitInfo.signalSemaphoreCount = signalSemaphoreCount;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}
		vkQueueSubmit(m_CommandQueue, 1, &submitInfo, fence);
	}

	void CommandPool::SubmitAndWait(uint32_t count, const VkCommandBuffer* cmds, VkFence fence)
	{
		Submit(count, cmds, fence);
		VK_CHECK(vkQueueWaitIdle(m_CommandQueue));
		vkFreeCommandBuffers(m_Device, m_CommandPool, (uint32_t)count, cmds);
	}


	/// ScopedCommandBuffer
	
	ScopedCommandBuffer::ScopedCommandBuffer(const Device& device, CommandPool* cmdPool) : pCmdPool{ cmdPool }
	{
		if (!cmdPool)
			pCmdPool = &g_CommandMgr.GetCommandPool();

		cmd = cmdPool->CreateCommandBuffer();
		Begin();
	}
	
	ScopedCommandBuffer::ScopedCommandBuffer(const Device& device, EQueueFamily queueFamily)
		: ScopedCommandBuffer(device, &g_CommandMgr.GetCommandPool(queueFamily))
	{
	}

	ScopedCommandBuffer::~ScopedCommandBuffer()
	{
		End();
		pCmdPool->SubmitAndWait(1, &cmd, nullptr);
	}

	void ScopedCommandBuffer::Begin(VkCommandBufferUsageFlags usage)
	{
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = usage;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
	}


	/// ScopedRendering

	ScopedRendering::ScopedRendering(VkCommandBuffer cmd, const VkRect2D& renderArea,
		const std::vector<std::pair<Image*, LoadStoreInfo>>& colorAttachments, const std::pair<Image*, LoadStoreInfo> &depthAttachment, bool beginCmd)
		: m_Cmd{ cmd }, m_RenderArea{ renderArea }, m_bBeginCmd{ beginCmd }
	{
		g_CommandContext.SetAttachments(colorAttachments, depthAttachment);
		Begin();
	}

	ScopedRendering::~ScopedRendering() 
	{
		End();
	}

	void ScopedRendering::Begin()
	{
		if (m_bBeginCmd)
			g_CommandContext.BeginCommandBuffer(m_Cmd);

		g_CommandContext.BeginRendering(m_Cmd, m_RenderArea);
	}

	void ScopedRendering::End() 
	{
		g_CommandContext.EndRendering(m_Cmd);

		if (m_bBeginCmd)
			g_CommandContext.EndCommandBuffer(m_Cmd);
	}


	/// CommandManager

	void CommandManager::Init(const Device& device)
	{
		uint32_t graphicsIndex = (uint32_t)EQueueFamily::Graphics;
		uint32_t computeIndex  = (uint32_t)EQueueFamily::Compute;
		uint32_t transferIndex = (uint32_t)EQueueFamily::Transfer;

		vkGetDeviceQueue(device, device.queueFamilyIndices.graphics, 0, &m_CommandQueues[graphicsIndex]);
		vkGetDeviceQueue(device, device.queueFamilyIndices.compute , 0, &m_CommandQueues[computeIndex ]);
		vkGetDeviceQueue(device, device.queueFamilyIndices.transfer, 0, &m_CommandQueues[transferIndex]);

		VkCommandPoolCreateFlags poolFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		m_CommandPools[graphicsIndex].Init(device, device.queueFamilyIndices.graphics, poolFlags, m_CommandQueues[graphicsIndex]);
		m_CommandPools[computeIndex ].Init(device, device.queueFamilyIndices.compute , poolFlags, m_CommandQueues[computeIndex ]);
		m_CommandPools[transferIndex].Init(device, device.queueFamilyIndices.transfer, poolFlags, m_CommandQueues[transferIndex]);		
	}

	void CommandManager::Cleanup(const Device& device)
	{
		for (uint32_t i = 0, imax = (uint32_t)EQueueFamily::Count; i < imax; ++i)
			m_CommandPools[i].Destroy();
	}

	VkCommandBuffer CommandManager::CreateCommandBuffer(const Device& device, EQueueFamily queueFamily, VkCommandBufferLevel level)
	{
		auto& pool = GetCommandPool(queueFamily);
		VkCommandBuffer cmd = pool.CreateCommandBuffer(level);
		return cmd;
	}

	VkCommandBuffer CommandManager::GetCommandBuffer(const Device& device, uint64_t fenceVal, EQueueFamily queueFamily, VkCommandBufferLevel level)
	{
		auto& pool = GetCommandPool(queueFamily);
		VkCommandBuffer cmd = pool.GetCommandBuffer(fenceVal, level);
		return cmd;
	}


	/// CommandContext

	CommandContext::Attachment::Attachment(const Image& image) : view{image.views[0]}, layout{image.layout}
	{
	}

	void CommandContext::UpdateDescriptorSetInfo(const Pipeline& pipeline)
	{
		for (uint32_t i = 0; i < Pipeline::s_MaxDescrptorSetNum; ++i)
			pipeline.UpdateDescriptorSetInfo(descriptorSetInfos[i], i);
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

	void CommandContext::SetAttachments(Attachment* pColorAttachments, uint32_t colorAttachmentCount, LoadStoreInfo* pColorLoadStoreInfos, VkClearColorValue* pClearColorValues,
		Attachment* pDepthAttachment, LoadStoreInfo* pDepthLoadStoreInfo, VkClearDepthStencilValue* pClearDepthValue)
	{
		assert(colorAttachmentCount < s_MaxAttachments);

		activeColorAttachmentCount = colorAttachmentCount;

		if (pColorAttachments != nullptr)
		{
			std::copy_n(pColorAttachments, colorAttachmentCount, cachedColorAttachments);

			if (pColorLoadStoreInfos != nullptr)
				std::copy_n(pColorLoadStoreInfos, colorAttachmentCount, cachedColorLoadStoreInfos);

			if (pClearColorValues != nullptr)
				for (uint32_t i = 0; i < colorAttachmentCount; ++i)
					cachedColorClearValues[i].color = pClearColorValues[i];
			else
				for (uint32_t i = 0; i < colorAttachmentCount; ++i)
					cachedColorClearValues[i].color = VkClearColorValue{};
		}

		if (pDepthAttachment != nullptr)
		{
			cachedDepthAttachment = *pDepthAttachment;

			if (pDepthLoadStoreInfo != nullptr)
				cachedDepthLoadStoreInfo = *pDepthLoadStoreInfo;

			if (pClearDepthValue)
				cachedDepthClearValue.depthStencil = *pClearDepthValue;
		}
		else
		{
			cachedDepthAttachment.view = VK_NULL_HANDLE;
		}

		// TODO:
		cachedDepthResolve.view = nullptr;
	}

	void CommandContext::SetAttachments(const std::vector<std::pair<Image*, LoadStoreInfo>>& colorAttachments, const std::pair<Image*, LoadStoreInfo>& depthAttachment)
	{
		uint32_t colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());

		assert(colorAttachmentCount < s_MaxAttachments);

		activeColorAttachmentCount = colorAttachmentCount;

		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			cachedColorAttachments[i].view = colorAttachments[i].first->views[0];
			cachedColorAttachments[i].layout = colorAttachments[i].first->layout;

			cachedColorLoadStoreInfos[i] = colorAttachments[i].second;
			cachedColorClearValues[i] = colorAttachments[i].first->clearValue;
		}

		if (depthAttachment.first != nullptr)
		{
			cachedDepthAttachment.view = depthAttachment.first->views[0];
			cachedDepthAttachment.layout = depthAttachment.first->layout;

			cachedDepthLoadStoreInfo = depthAttachment.second;
			cachedDepthClearValue.depthStencil = depthAttachment.first->clearValue.depthStencil;
		}
		else
		{
			cachedDepthAttachment.view = VK_NULL_HANDLE;
		}

		// TODO:
		cachedDepthResolve.view = nullptr;
	}

	void CommandContext::BeginRendering(VkCommandBuffer cmd, const VkRect2D& renderArea)
	{
		// Colors
		std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;

		if (activeColorAttachmentCount > 0)
		{
			colorAttachmentInfos.resize(activeColorAttachmentCount);

			for (uint32_t i = 0; i < activeColorAttachmentCount; ++i)
			{
				auto& attachmentInfo = colorAttachmentInfos[i];

				attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				attachmentInfo.imageView = cachedColorAttachments[i].view;
				attachmentInfo.imageLayout = cachedColorAttachments[i].layout;
				attachmentInfo.loadOp = cachedColorLoadStoreInfos[i].loadOp;
				attachmentInfo.storeOp = cachedColorLoadStoreInfos[i].storeOp;
				attachmentInfo.clearValue = cachedColorClearValues[i];

				if (activeColorResolveCount > 0)
				{
					attachmentInfo.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
					attachmentInfo.resolveImageView = cachedColorResolves[i].view;
					attachmentInfo.resolveImageLayout = cachedColorResolves[i].layout;
				}
			}
		}

		// Depth
		// A single depth stencil attachment info can be used, but they can also be specified separately.
		// When both are specified separately, the only requirement is that the image view is identical.
		VkRenderingAttachmentInfo depthAttachmentInfo{};
		depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

		if (cachedDepthAttachment.view != nullptr)
		{
			depthAttachmentInfo.imageView = cachedDepthAttachment.view;
			depthAttachmentInfo.imageLayout = cachedDepthAttachment.layout;
			depthAttachmentInfo.loadOp = cachedDepthLoadStoreInfo.loadOp;
			depthAttachmentInfo.storeOp = cachedDepthLoadStoreInfo.storeOp;
			depthAttachmentInfo.clearValue = cachedDepthClearValue;

			if (cachedDepthResolve.view != nullptr)
			{
				depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_MAX_BIT;
				depthAttachmentInfo.resolveImageView = cachedDepthResolve.view;
				depthAttachmentInfo.resolveImageLayout = cachedDepthResolve.layout;
			}
		}
		
		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.flags = 0;
		renderingInfo.renderArea = renderArea;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = activeColorAttachmentCount;
		renderingInfo.pColorAttachments = activeColorAttachmentCount > 0 ? colorAttachmentInfos.data() : nullptr;
		renderingInfo.pDepthAttachment = depthAttachmentInfo.imageView != VK_NULL_HANDLE ? &depthAttachmentInfo : nullptr;
		renderingInfo.pStencilAttachment = depthAttachmentInfo.imageView != VK_NULL_HANDLE ? &depthAttachmentInfo : nullptr;

		vkCmdBeginRendering(cmd, &renderingInfo);
	}

	void CommandContext::EndRendering(VkCommandBuffer cmd)
	{
		vkCmdEndRendering(cmd);

		// Clear dynamic rendering caches
		activeColorAttachmentCount = 0;
		activeColorResolveCount = 0;
		cachedDepthAttachment.view = nullptr;
		cachedDepthResolve.view = nullptr;
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
		assert(pipeline.pipeline);

		if (cachedPipeline == &pipeline)
			return;

		pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vkCmdBindPipeline(cmd, pipelineBindPoint, pipeline.pipeline);

		cachedPipeline = &pipeline;

		UpdateDescriptorSetInfo(pipeline);
	}

	void CommandContext::BindPipeline(VkCommandBuffer cmd, const ComputePipeline& pipeline)
	{
		if (cachedPipeline == &pipeline)
			return;

		pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
		vkCmdBindPipeline(cmd, pipelineBindPoint, pipeline.pipeline);

		cachedPipeline = &pipeline;

		UpdateDescriptorSetInfo(pipeline);
	}

	std::vector<DescriptorInfo> CommandContext::GetDescriptorInfos(uint32_t set) const
	{
		std::vector<DescriptorInfo> descriptorInfos;

		const auto& descriptorSetInfo = descriptorSetInfos[set];

		if (descriptorSetInfo.count == 0)
			return descriptorInfos;

		descriptorInfos.resize(descriptorSetInfo.count);
		for (uint32_t i = 0; i < descriptorSetInfo.count; ++i)
		{
			uint32_t index = i + descriptorSetInfo.start;
			if (descriptorSetInfo.mask & (1 << index))
				descriptorInfos[i] = cachedDescriptorInfos[set][index];
		}

		return descriptorInfos;
	}

	std::vector<VkWriteDescriptorSet> CommandContext::GetWriteDescriptorSets(uint32_t set) const
	{
		std::vector<VkWriteDescriptorSet> descriptorSets;

		const auto& descriptorSetInfo = descriptorSetInfos[set];

		if (descriptorSetInfo.count == 0)
			return descriptorSets;

		VkWriteDescriptorSet descriptorSet{};
		descriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		
		for (uint32_t i = descriptorSetInfo.start, imax = descriptorSetInfo.start + descriptorSetInfo.count; i < imax; ++i)
		{
			if (descriptorSetInfo.mask & (1 << i))
			{
				descriptorSet.dstBinding = i;
				descriptorSet.descriptorCount = 1;
				descriptorSet.descriptorType = descriptorSetInfos[set].types[i];
				descriptorSet.pBufferInfo = &(cachedDescriptorInfos[set][i].bufferInfo);
				descriptorSet.pImageInfo = &(cachedDescriptorInfos[set][i].imageInfo);

				descriptorSets.push_back(descriptorSet);
			}
		}

		return descriptorSets;
	}

	void CommandContext::PushConstants(VkCommandBuffer cmd, const std::string& name, uint32_t offset, uint32_t size, void* pValues)
	{
		assert(cachedPipeline || pValues != nullptr);

		if (!cachedPipeline->pushConstants.empty())
		{
			auto iter = cachedPipeline->pushConstants.find(name);
			if (iter != cachedPipeline->pushConstants.end())
			{
				vkCmdPushConstants(cmd, cachedPipeline->layout, iter->second.stages, offset, size, pValues);
			}
		}
	}

	void CommandContext::ImageBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
	{
		assert(activeImageMemoryBarriers < s_MaxBarrierNum);

		auto& barrier = cachedImageMemoryBarriers[activeImageMemoryBarriers++];
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.subresourceRange = { aspectFlags, 0, 1, 0, 1 };
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::ImageBarrier(VkImage image, const VkImageSubresourceRange& subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
	{
		assert(activeImageMemoryBarriers < s_MaxBarrierNum);

		auto& barrier = cachedImageMemoryBarriers[activeImageMemoryBarriers++];
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.subresourceRange = subresourceRange;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::BufferBarrier(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
	{
		assert(activeBufferMemoryBarriers < s_MaxBarrierNum);

		auto& barrier = cachedBufferMemoryBarriers[activeBufferMemoryBarriers++];
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.buffer = buffer;
		barrier.size = size;
		barrier.offset = offset;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::PipelineBarriers(VkCommandBuffer cmd, VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask)
	{
		if (activeBufferMemoryBarriers > 0 || activeImageMemoryBarriers > 0)
		{
			const auto pBufferMemoryBarriers = activeBufferMemoryBarriers > 0 ? cachedBufferMemoryBarriers : nullptr;
			const auto pImageMemoryBarriers = activeImageMemoryBarriers > 0 ? cachedImageMemoryBarriers : nullptr;

			vkCmdPipelineBarrier(cmd, srcMask, dstMask, 0,
				0, nullptr,
				activeBufferMemoryBarriers, pBufferMemoryBarriers,
				activeImageMemoryBarriers, pImageMemoryBarriers);

			activeBufferMemoryBarriers = 0;
			activeImageMemoryBarriers = 0;
		}
	}

	void CommandContext::ImageBarrier2(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask)
	{
		assert(activeImageMemoryBarriers2 < s_MaxBarrierNum);

		auto& barrier2 = cachedImageMemoryBarriers2[activeImageMemoryBarriers2++];
		barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier2.image = image;
		barrier2.subresourceRange = { aspectFlags, 0, 1, 0, 1 };
		barrier2.oldLayout = oldLayout;
		barrier2.newLayout = newLayout;
		barrier2.srcAccessMask = srcAccessMask;
		barrier2.dstAccessMask = dstAccessMask;
		barrier2.srcStageMask = srcStageMask;
		barrier2.dstStageMask = dstStageMask;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::ImageBarrier2(VkImage image, const VkImageSubresourceRange& subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask)
	{
		assert(activeImageMemoryBarriers2 < s_MaxBarrierNum);

		auto& barrier2 = cachedImageMemoryBarriers2[activeImageMemoryBarriers2++];
		barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier2.image = image;
		barrier2.subresourceRange = subresourceRange;
		barrier2.oldLayout = oldLayout;
		barrier2.newLayout = newLayout;
		barrier2.srcAccessMask = srcAccessMask;
		barrier2.dstAccessMask = dstAccessMask;
		barrier2.srcStageMask = srcStageMask;
		barrier2.dstStageMask = dstStageMask;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::ImageBarrier2(Image &image, VkImageLayout newLayout, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask)
	{
		assert(activeImageMemoryBarriers2 < s_MaxBarrierNum);

		auto& barrier2 = cachedImageMemoryBarriers2[activeImageMemoryBarriers2++];
		barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier2.image = image;
		barrier2.subresourceRange = image.views[0].subresourceRange;
		barrier2.oldLayout = image.layout;
		barrier2.newLayout = image.layout = newLayout;
		barrier2.srcAccessMask = srcAccessMask;
		barrier2.dstAccessMask = dstAccessMask;
		barrier2.srcStageMask = srcStageMask;
		barrier2.dstStageMask = dstStageMask;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::BufferBarrier2(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask)
	{
		assert(activeBufferMemoryBarriers2 < s_MaxBarrierNum);

		auto& barrier2 = cachedBufferMemoryBarriers2[activeBufferMemoryBarriers2++];
		barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier2.buffer = buffer;
		barrier2.size = size;
		barrier2.offset = offset;
		barrier2.srcAccessMask = srcAccessMask;
		barrier2.dstAccessMask = dstAccessMask;
		barrier2.srcStageMask = srcStageMask;
		barrier2.dstStageMask = dstStageMask;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void CommandContext::PipelineBarriers2(VkCommandBuffer cmd)
	{
		if (activeBufferMemoryBarriers2 > 0 || activeImageMemoryBarriers2 > 0)
		{
			const auto pBufferMemoryBarriers2 = activeBufferMemoryBarriers2 > 0 ? cachedBufferMemoryBarriers2 : nullptr;
			const auto pImageMemoryBarriers2 = activeImageMemoryBarriers2 > 0 ? cachedImageMemoryBarriers2 : nullptr;

			VkDependencyInfo dependencyInfo{};
			dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			
			dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			dependencyInfo.bufferMemoryBarrierCount = activeBufferMemoryBarriers2;
			dependencyInfo.pBufferMemoryBarriers = pBufferMemoryBarriers2;
			dependencyInfo.imageMemoryBarrierCount = activeImageMemoryBarriers2;
			dependencyInfo.pImageMemoryBarriers = pImageMemoryBarriers2;

			vkCmdPipelineBarrier2(cmd, &dependencyInfo);

			activeBufferMemoryBarriers2 = 0;
			activeImageMemoryBarriers2 = 0;
		}
	}

	void CommandContext::Blit(VkCommandBuffer cmd, const Image& srcImage, const Image& dstImage, uint32_t srcMipLevel, uint32_t dstMipLevel)
	{
		VkImageBlit blit{ };
		blit.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
		blit.srcOffsets[1] = VkOffset3D{ (int32_t)srcImage.extent.width, (int32_t)srcImage.extent.height, 1 };
		blit.srcSubresource = { srcImage.subresource.aspectMask, srcMipLevel, 0, 1 };
		blit.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
		blit.dstOffsets[1] = VkOffset3D{ (int32_t)dstImage.extent.width, (int32_t)dstImage.extent.height, 1 };
		blit.dstSubresource = { dstImage.subresource.aspectMask, dstMipLevel, 0, 1 };

		vkCmdBlitImage(cmd,
			srcImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_LINEAR);
	}

	void CommandContext::Blit(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkRect2D srcRegion, VkRect2D dstRegion, uint32_t srcMipLevel, uint32_t dstMipLevel)
	{
		VkImageBlit blit{ };
		blit.srcOffsets[0] = VkOffset3D{ srcRegion.offset.x, srcRegion.offset.y, 0 };
		blit.srcOffsets[1] = VkOffset3D{ srcRegion.offset.x + (int32_t)srcRegion.extent.width, srcRegion.offset.y + (int32_t)srcRegion.extent.height, 1 };
		blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, srcMipLevel, 0, 1 };
		blit.dstOffsets[0] = VkOffset3D{ dstRegion.offset.x, dstRegion.offset.y, 0 };
		blit.dstOffsets[1] = VkOffset3D{ dstRegion.offset.x + (int32_t)dstRegion.extent.width, dstRegion.offset.y + (int32_t)dstRegion.extent.height, 1 };
		blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, dstMipLevel, 0, 1 };

		vkCmdBlitImage(cmd,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_LINEAR);
	}
}
