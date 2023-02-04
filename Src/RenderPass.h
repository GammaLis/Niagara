#pragma once

#include "pch.h"
#include "VkCommon.h"


namespace Niagara
{
	class Device;

	struct Attachment
	{
		VkFormat format;
		VkImageUsageFlagBits usage;
		VkImageLayout layout;
		VkSampleCountFlagBits samples;

		explicit Attachment(VkFormat inFormat = VK_FORMAT_UNDEFINED, VkImageUsageFlagBits inUsage = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout inLayout = VK_IMAGE_LAYOUT_UNDEFINED, VkSampleCountFlagBits inSamples = VK_SAMPLE_COUNT_1_BIT)
			: format{inFormat}, usage{inUsage}, layout{inLayout}, samples{inSamples}
		{  }
	};

	struct SubpassInfo
	{
		std::vector<uint32_t> inputAttachments;
		std::vector<uint32_t> outputAttachments;
		std::vector<uint32_t> colorResolveAttachments;
		
		bool bHasDepthStencilAttachment{ true };
		uint32_t depthStencilResolveAttachment{ 0 };
		VkResolveModeFlagBits depthStencilResolveMode{ VK_RESOLVE_MODE_NONE };
	};


	/// Render pass

	VkRenderPass GetRenderPass(VkDevice device, VkFormat format);

	std::vector<VkAttachmentDescription> GetAttachmentDescriptions(const std::vector<Attachment>& attachments, const std::vector<LoadStoreInfo>& loadStoreInfos);
	void SetAttachmentLayouts(std::vector<VkSubpassDescription>& subpassDescriptions, std::vector<VkAttachmentDescription>& attachmentDescriptions);

	class RenderPass
	{
	private:
		std::vector<Attachment> attachments;
		std::vector<LoadStoreInfo> loadStoreInfos;
		std::vector<SubpassInfo> subpasses;

		void UpdateAttachments();

	public:
		std::vector<Attachment> colorAttachments;
		std::vector<LoadStoreInfo> colorLoadStoreInfos;
		uint32_t activeColorAttachmentCount{ 0 };

		Attachment depthAttachment;
		LoadStoreInfo depthLoadStoreInfo;

		VkRenderPass renderPass = VK_NULL_HANDLE;
		std::vector<uint32_t> colorOutputCounts;
		uint32_t subpassCount{ 1 };

		RenderPass() = default;

		void Init(const Device& device);
		void Destroy(const Device& device);

		operator VkRenderPass() const { return renderPass; }

		void SetAttachments(Attachment* pColorAttachments, uint32_t colorAttachmentCount, LoadStoreInfo* pColorLoadStoreInfos, Attachment* pDepthAttachment = nullptr, LoadStoreInfo* pDepthLoadStoreInfo = nullptr)
		{
			activeColorAttachmentCount = colorAttachmentCount;

			if (pColorAttachments != nullptr)
			{
				if (colorAttachments.size() < colorAttachmentCount)
				{
					colorAttachments.resize(colorAttachmentCount);
					colorLoadStoreInfos.resize(colorAttachmentCount);
				}

				std::copy_n(pColorAttachments, colorAttachmentCount, colorAttachments.begin());

				if (pColorLoadStoreInfos != nullptr)
					std::copy_n(pColorLoadStoreInfos, colorAttachmentCount, colorLoadStoreInfos.begin());
			}

			if (pDepthAttachment != nullptr)
			{
				depthAttachment = *pDepthAttachment;

				if (pDepthLoadStoreInfo != nullptr)
					depthLoadStoreInfo = *pDepthLoadStoreInfo;
			}
			else
			{
				depthAttachment.format = VK_FORMAT_UNDEFINED;
			}
		}

		void SetSubpassInfos(const std::vector<SubpassInfo>& subpassInfos)
		{
			subpasses = subpassInfos;
		}

		uint32_t GetAttachmentCount() const { return static_cast<uint32_t>(attachments.size()); }
	};
}
