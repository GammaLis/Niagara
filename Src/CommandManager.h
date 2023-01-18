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

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags);
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
	private:
		VkCommandBuffer cachedCommandBuffer = VK_NULL_HANDLE;
		VkRenderPass cachedRenderPass = VK_NULL_HANDLE;

		VkPipelineBindPoint pipelineBindPoint = (VkPipelineBindPoint)0;

		// TODO: Are these needed ? 
		VkPipelineLayout cachedPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorUpdateTemplate cachedDescriptorUpdateTemplate = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayout> cachedDescriptorSetLayouts;

		DescriptorSetInfo descriptorSetInfos[Pipeline::s_MaxDescrptorSetNum] = {};
		DescriptorInfo cachedDescriptorInfos[Pipeline::s_MaxDescrptorSetNum][Pipeline::s_MaxDescriptorNum] = {};
		VkWriteDescriptorSet cachedWriteDescriptorSets[Pipeline::s_MaxDescrptorSetNum][Pipeline::s_MaxDescriptorNum] = {};

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

		void SetDescriptor(const DescriptorInfo& info, uint32_t binding, uint32_t set = 0)
		{
			assert(descriptorSetInfos[set].mask & (1 << binding));
			// TODO:
			// assert(descriptorTypes[index] == buffer.Type)
			cachedDescriptorInfos[set][binding] = info;
		}

		void SetDescriptor(const VkWriteDescriptorSet& descriptor, uint32_t binding, uint32_t set = 0)
		{
			assert(descriptorSetInfos[set].mask & (1 << binding));

			cachedWriteDescriptorSets[set][binding] = descriptor;
		}

		std::vector<DescriptorInfo> GetDescriptorInfos(uint32_t set = 0) const;

		std::vector<VkWriteDescriptorSet> GetDescriptorSets(uint32_t set = 0) const;

		void PushDescriptorSetWithTemplate(VkCommandBuffer cmd, uint32_t set = 0)
		{
			assert(cachedDescriptorUpdateTemplate);

			auto descriptorInfos = GetDescriptorInfos(set);
			vkCmdPushDescriptorSetWithTemplateKHR(cmd, cachedDescriptorUpdateTemplate, cachedPipelineLayout, set, descriptorInfos.data());
		}

		void PushDescriptorSet(VkCommandBuffer cmd, uint32_t set = 0)
		{
			auto writeDescriptorSets = GetDescriptorSets(set);
			vkCmdPushDescriptorSetKHR(cmd, pipelineBindPoint, cachedPipelineLayout, set, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
		}

	};

	extern CommandContext g_CommandContext;
}
