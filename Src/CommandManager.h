#pragma once

#include "pch.h"
#include "VkCommon.h"
#include "Shaders.h"
#include "Pipeline.h"
#include <queue>


namespace Niagara
{
	class Device;
	class Image;
	class ImageView;

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
		static constexpr uint32_t s_MaxAttachments = 8;

		struct Attachment
		{
			const ImageView* view{ nullptr };
			VkImageLayout layout{ VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL };

			Attachment() = default;

			Attachment(const ImageView *inView, VkImageLayout inLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) 
				: view{inView}, layout {inLayout}
			{  }
		};

	private:
		VkCommandBuffer cachedCommandBuffer = VK_NULL_HANDLE;
		VkRenderPass cachedRenderPass = VK_NULL_HANDLE;

		const Pipeline *cachedPipeline = nullptr;

		VkPipelineBindPoint pipelineBindPoint = (VkPipelineBindPoint)0;
		
		// Descriptors
		DescriptorSetInfo descriptorSetInfos[Pipeline::s_MaxDescrptorSetNum] = {};
		DescriptorInfo cachedDescriptorInfos[Pipeline::s_MaxDescrptorSetNum][Pipeline::s_MaxDescriptorNum] = {};
		VkWriteDescriptorSet cachedWriteDescriptorSets[Pipeline::s_MaxDescrptorSetNum][Pipeline::s_MaxDescriptorNum] = {};

		// Dynamic rendering
		// Attachments
		Attachment cachedColorAttachments[s_MaxAttachments];
		LoadStoreInfo cachedColorLoadStoreInfos[s_MaxAttachments];
		VkClearValue cachedColorClearValues[s_MaxAttachments] = {};
		uint32_t activeColorAttachmentCount{ 0 };
		Attachment cachedColorResolves[s_MaxAttachments];
		uint32_t activeColorResolveCount{ 0 };

		Attachment cachedDepthAttachment;
		LoadStoreInfo cachedDepthLoadStoreInfo;
		VkClearValue cachedDepthClearValue{};
		Attachment cachedDepthResolve;

		// Barriers
		VkImageMemoryBarrier cachedImageMemoryBarriers[s_MaxBarrierNum] = {};
		uint32_t activeImageMemoryBarriers = 0;
		VkBufferMemoryBarrier cachedBufferMemoryBarriers[s_MaxBarrierNum] = {};
		uint32_t activeBufferMemoryBarriers = 0;

		// Barriers2
		VkImageMemoryBarrier2 cachedImageMemoryBarriers2[s_MaxBarrierNum] = {};
		uint32_t activeImageMemoryBarriers2 = 0;
		VkBufferMemoryBarrier2 cachedBufferMemoryBarriers2[s_MaxBarrierNum] = {};
		uint32_t activeBufferMemoryBarriers2 = 0;

		void UpdateDescriptorSetInfo(const Pipeline& pipeline);

	public:
		void BeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags usage = 0);

		void EndCommandBuffer(VkCommandBuffer cmd)
		{
			VK_CHECK(vkEndCommandBuffer(cmd));

			cachedCommandBuffer = VK_NULL_HANDLE;
		}

		// Dynamic rendering
		void SetAttachments(Attachment* pColorAttachments, uint32_t colorAttachmentCount, LoadStoreInfo* pColorLoadStoreInfos, VkClearColorValue *pClearColorValues = nullptr,
			Attachment* pDepthAttachment = nullptr, LoadStoreInfo* pDepthLoadStoreInfo = nullptr, VkClearDepthStencilValue *pClearDepthValue = nullptr);

		void BeginRendering(VkCommandBuffer cmd, const VkRect2D& renderArea);

		void EndRendering(VkCommandBuffer cmd);

		// Render pass
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

		// Barriers

		void ImageBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT);

		void ImageBarrier(VkImage image, const VkImageSubresourceRange& subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT);

		void BufferBarrier(VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkAccessFlags srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, VkAccessFlags dstAccessMask = VK_ACCESS_MEMORY_READ_BIT);

		void PipelineBarriers(VkCommandBuffer cmd, VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask);

		// Barriers2

		void ImageBarrier2(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout,
			VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
			VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void ImageBarrier2(VkImage image, const VkImageSubresourceRange& subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout,
			VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
			VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void BufferBarrier2(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,
			VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
			VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void PipelineBarriers2(VkCommandBuffer cmd);

		void Blit(VkCommandBuffer cmd, const Image& srcImage, const Image& dstImage, uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0);

		void Blit(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkRect2D srcRegion, VkRect2D dstRegion, uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0);

	};

	extern CommandContext g_CommandContext;
}
