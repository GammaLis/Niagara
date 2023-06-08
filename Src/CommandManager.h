#pragma once

#include "pch.h"
#include "VkCommon.h"
#include "Shaders.h"
#include "Pipeline.h"
#include "Utilities.h"
#include <queue>


namespace Niagara
{
	class Device;
	class Image;
	class Texture;
	class ImageView;
	class Buffer;

	struct AccessedAttachment;

	enum class EQueueFamily
	{
		Graphics,
		Compute,
		Transfer,

		Count
	};

	VkCommandBuffer BeginSingleTimeCommands(EQueueFamily queueFamily = EQueueFamily::Graphics);
	void EndSingleTimeCommands(VkCommandBuffer cmd, EQueueFamily queueFamily = EQueueFamily::Graphics);

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags, VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags, EQueueFamily queueFamily = EQueueFamily::Graphics);
	void InitializeTexture(const Device &device, Texture& texture, const void *pInitData, size_t size);
	void ClearImage(VkCommandBuffer cmd, Image& image);
	void BlitImage(VkCommandBuffer cmd, const Image& src, const Image& dst,
		const VkOffset3D& srcOffset, const VkOffset3D& dstOffset, const VkExtent3D& srcExtent, const VkExtent3D& dstExtent,
		uint32_t srcLevel, uint32_t dstLevel, uint32_t srcBaseLayer = 0, uint32_t dstBaseLayer = 0, VkFilter filter = VK_FILTER_LINEAR, uint32_t numLayers = 1);
	void GenerateMipmap(VkCommandBuffer cmd, Image& image);


	// Ref: nvpro_nvvk
	class CommandPool
	{
	public:
		CommandPool() = default;
		NON_COPYABLE(CommandPool);

		// If defaultQueue is null, uses the first queue from familyIndex as default
		void Init(const Device& device, uint32_t familyIndex, VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, VkQueue defaultQueue = VK_NULL_HANDLE);
		void Destroy();
		
		VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		void Free(uint32_t count, const VkCommandBuffer* cmds);

		VkCommandBuffer GetCommandBuffer(uint64_t fenceVal, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		void Submit(uint32_t count, const VkCommandBuffer* cmds, VkFence fence = VK_NULL_HANDLE,
			uint32_t waitSemaphoreCount = 0, VkSemaphore *waitSemaphores = nullptr, VkPipelineStageFlags* waitStages = nullptr,
			uint32_t signalSemaphoreCount = 0, VkSemaphore *signalSemaphores = nullptr);
		void SubmitAndWait(uint32_t count, const VkCommandBuffer* cmds, VkFence fence = VK_NULL_HANDLE);

		operator VkCommandPool() const { return m_CommandPool; }
		
	private:
		VkDevice m_Device{ VK_NULL_HANDLE };
		VkCommandPool m_CommandPool{ VK_NULL_HANDLE };
		VkQueue m_CommandQueue{ VK_NULL_HANDLE };

		std::queue<std::pair<uint64_t, VkCommandBuffer>> m_UsedCmds;
		std::queue<VkCommandBuffer> m_FreeCmds;
	};

	class ScopedCommandBuffer 
	{
	public:
		ScopedCommandBuffer(const Device& device, CommandPool* cmdPool = nullptr);
		ScopedCommandBuffer(const Device& device, EQueueFamily queueFamily = EQueueFamily::Graphics);

		~ScopedCommandBuffer();

		void Begin(VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		void End() { vkEndCommandBuffer(cmd); }

		operator VkCommandBuffer() const { return cmd; }

	private:
		VkCommandBuffer cmd{ VK_NULL_HANDLE };
		CommandPool* pCmdPool{ nullptr };
	};

	class ScopedRendering
	{
	public:
		ScopedRendering(VkCommandBuffer cmd, const VkRect2D &renderArea, 
			const std::vector<std::pair<Image*, LoadStoreInfo>>& colorAttachments, const std::pair<Image*, LoadStoreInfo>& depthAttachments = {nullptr, LoadStoreInfo()}, bool beginCmd = false);
		~ScopedRendering();

		NON_COPYABLE(ScopedRendering);

		void Begin();
		void End();

	private:
		VkCommandBuffer m_Cmd{ VK_NULL_HANDLE };
		VkRect2D m_RenderArea;
		bool m_bBeginCmd{ false };
	};


	class CommandManager
	{
	public:
		static constexpr uint32_t s_QueueCount = (uint32_t)EQueueFamily::Count;

		void Init(const Device& device);
		void Cleanup(const Device& device);

		VkCommandBuffer CreateCommandBuffer(const Device& device, EQueueFamily queueType = EQueueFamily::Graphics, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		VkCommandBuffer GetCommandBuffer(const Device &device, uint64_t fenceVal, EQueueFamily queueType = EQueueFamily::Graphics, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		VkQueue GraphicsQueue() const { return m_CommandQueues[(int32_t)EQueueFamily::Graphics]; }
		VkQueue ComputeQueue () const { return m_CommandQueues[(int32_t)EQueueFamily::Compute ]; }
		VkQueue TransferQueue() const { return m_CommandQueues[(int32_t)EQueueFamily::Transfer]; }

		VkQueue GetCommandQueue(EQueueFamily queueFamily) const { return m_CommandQueues[(uint32_t)queueFamily]; }

		CommandPool& GetCommandPool(EQueueFamily queueFamily = EQueueFamily::Graphics, uint32_t index = 0) 
		{
			return m_CommandPools[(uint32_t)queueFamily];
		}

	private:
		std::queue<VkCommandBuffer> m_CommandBuffers;

		VkQueue m_CommandQueues[s_QueueCount];
		CommandPool m_CommandPools[s_QueueCount];
	};
	extern CommandManager g_CommandMgr;


	class CommandContext
	{
	public:
		static constexpr uint32_t s_MaxBarrierNum = 16;
		static constexpr uint32_t s_MaxAttachments = 8;

		struct Attachment
		{
			VkImageView view{ VK_NULL_HANDLE };
			VkImageLayout layout{ VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL };

			Attachment() = default;
			Attachment(VkImageView inView, VkImageLayout inLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) 
				: view{inView}, layout {inLayout}
			{  }
			explicit Attachment(const Image& image);
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
		void Invalidate() 
		{
			cachedPipeline = nullptr;
			cachedCommandBuffer = VK_NULL_HANDLE;
		}

		void BeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags usage = 0);

		void EndCommandBuffer(VkCommandBuffer cmd)
		{
			VK_CHECK(vkEndCommandBuffer(cmd));

			cachedCommandBuffer = VK_NULL_HANDLE;
		}

		// Dynamic rendering
		void SetAttachments(Attachment* pColorAttachments, uint32_t colorAttachmentCount, LoadStoreInfo* pColorLoadStoreInfos, VkClearColorValue *pClearColorValues = nullptr,
			Attachment* pDepthAttachment = nullptr, LoadStoreInfo* pDepthLoadStoreInfo = nullptr, VkClearDepthStencilValue *pClearDepthValue = nullptr);
		void SetAttachments(const std::vector<std::pair<Image*, LoadStoreInfo>> &colorAttachments, const std::pair<Image*, LoadStoreInfo> &depthAttachment);
		void SetAttachments(const std::vector<AccessedAttachment> &colorAttachments, const AccessedAttachment &depthAttachment);

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
			descriptor.pImageInfo = &info.imageInfo;
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

		// Deprecated
		void ImageBarrier2_Deprecated(Image &image, VkImageLayout newLayout,
			VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
			VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void ImageBarrier2(Image& image, VkImageLayout newLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void BufferBarrier2(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,
			VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
			VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void BufferBarrier2(Buffer &buffer, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT);

		void PipelineBarriers2(VkCommandBuffer cmd);

		void Blit(VkCommandBuffer cmd, const Image& srcImage, const Image& dstImage, uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0);

		void Blit(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkRect2D srcRegion, VkRect2D dstRegion, uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0);

	};
	extern CommandContext g_CommandContext;
}
