#pragma once

#include "pch.h"
#include "Shaders.h"
#include "Pipeline.h"
#include <queue>


namespace Niagara
{
	class Device;

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer cmd);

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags, VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags);

	class CommandManager
	{
	public:
		static VkCommandBuffer CreateCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		void Init(const Device& device);
		void Cleanup(const Device& device);

		VkCommandBuffer GetCommandBuffer(const Device& device);

		VkQueue graphicsQueue;
		VkQueue computeQueue;
		VkQueue transferQueue;

		VkCommandPool commandPool;

	private:
		std::queue<VkCommandBuffer> m_CommandBuffers;
	};

	extern CommandManager g_CommandMgr;


	class CommandContext
	{
	public:
		static constexpr uint32_t s_MaxBarrierNum = 16;

	private:
		VkCommandBuffer cachedCommandBuffer = VK_NULL_HANDLE;
		VkRenderPass cachedRenderPass = VK_NULL_HANDLE;

		const Pipeline *cachedPipeline = nullptr;

		VkPipelineBindPoint pipelineBindPoint = (VkPipelineBindPoint)0;
		
		DescriptorSetInfo descriptorSetInfos[Pipeline::s_MaxDescrptorSetNum] = {};
		DescriptorInfo cachedDescriptorInfos[Pipeline::s_MaxDescrptorSetNum][Pipeline::s_MaxDescriptorNum] = {};
		VkWriteDescriptorSet cachedWriteDescriptorSets[Pipeline::s_MaxDescrptorSetNum][Pipeline::s_MaxDescriptorNum] = {};

		// Barriers
		VkImageMemoryBarrier cachedImageMemoryBarriers[s_MaxBarrierNum] = {};
		uint32_t activeImageMemoryBarriers = 0;
		VkBufferMemoryBarrier cachedBufferMemoryBarriers[s_MaxBarrierNum] = {};
		uint32_t activeBufferMemoryBarriers = 0;

		void UpdateDescriptorSetInfo(const Pipeline& pipeline);

	public:
		void BeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags usage = 0);

		void EndCommandBuffer(VkCommandBuffer cmd)
		{
			VK_CHECK(vkEndCommandBuffer(cmd));

			cachedCommandBuffer = VK_NULL_HANDLE;
		}

		void BeginRenderPass(VkCommandBuffer cmd, VkRenderPass renderPass, VkFramebuffer framebuffer, const VkRect2D& renderArea, const std::vector<VkClearValue>& clearValues);

		void EndRenderPass(VkCommandBuffer cmd)
		{
			vkCmdEndRenderPass(cmd);

			cachedRenderPass = VK_NULL_HANDLE;
		}

		void BindPipeline(VkCommandBuffer cmd, const GraphicsPipeline& pipeline);

		void BindPipeline(VkCommandBuffer cmd, const ComputePipeline& pipeline);

		void SetDescriptor(uint32_t binding, const DescriptorInfo& info, uint32_t set = 0)
		{
			assert(descriptorSetInfos[set].mask & (1 << binding));
			// TODO:
			// assert(descriptorTypes[index] == buffer.Type)
			cachedDescriptorInfos[set][binding] = info;
		}

		void SetDescriptor(uint32_t binding, const VkWriteDescriptorSet& descriptor, uint32_t set = 0)
		{
			assert(descriptorSetInfos[set].mask & (1 << binding));

			cachedWriteDescriptorSets[set][binding] = descriptor;
		}

		void SetWriteDescriptor(uint32_t binding, const DescriptorInfo& info, uint32_t set = 0)
		{
			assert(descriptorSetInfos[set].mask & (1 << binding));

			auto& descriptor = cachedWriteDescriptorSets[set][binding];
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstBinding = binding;
			descriptor.descriptorCount = 1;
			descriptor.descriptorType = descriptorSetInfos[set].types[binding];
			descriptor.pBufferInfo = &info.bufferInfo;
		}

		std::vector<DescriptorInfo> GetDescriptorInfos(uint32_t set = 0) const;

		std::vector<VkWriteDescriptorSet> GetWriteDescriptorSets(uint32_t set = 0) const;

		void PushDescriptorSetWithTemplate(VkCommandBuffer cmd, uint32_t set = 0)
		{
			assert(cachedPipeline && cachedPipeline->descriptorUpdateTemplate);

			auto descriptorInfos = GetDescriptorInfos(set);
			vkCmdPushDescriptorSetWithTemplateKHR(cmd, cachedPipeline->descriptorUpdateTemplate, cachedPipeline->layout, set, descriptorInfos.data());
		}

		void PushDescriptorSet(VkCommandBuffer cmd, uint32_t set = 0)
		{
			assert(cachedPipeline && cachedPipeline->descriptorUpdateTemplate);

			auto writeDescriptorSets = GetWriteDescriptorSets(set);
			vkCmdPushDescriptorSetKHR(cmd, pipelineBindPoint, cachedPipeline->layout, set, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
		}

		void PushConstants(VkCommandBuffer cmd, const std::string &name, uint32_t offset, uint32_t size, void *pValues);

		void ImageBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT)
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

		void ImageBarrier(VkImage image, const VkImageSubresourceRange &subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT)
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

		void BufferBarrier(VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT)
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

		void PipelineBarriers(VkCommandBuffer cmd, VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask)
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

		void Blit(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkRect2D srcRegion, VkRect2D dstRegion, uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0)
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

	};

	extern CommandContext g_CommandContext;
}
