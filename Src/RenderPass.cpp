#include "RenderPass.h"
#include "Device.h"


namespace Niagara
{
	VkRenderPass GetRenderPass(VkDevice device, VkFormat format)
	{
		// Attachment description
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// The `loadOp` and `storeOp` determine what to do with the data in the attachment before rendering and after rendering.
		// * LOAD: Preserve the existing contents of the attachments
		// * CLEAR: Clear the values to a constant at the start
		// * DONT_CARE: Existing contents are undefined; we don't care about them
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Subpasses and attachment references
		// A single render pass can consist of multiple subpasses. Subpasses are subsequent rendering operations that depend on
		// the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another.
		// If you group these rendering operations into one render pass, then Vulkan is able to reorder the operations and conserve memory bandwidth
		// for possibly better performance.
		VkAttachmentReference colorAttachementRef{};
		colorAttachementRef.attachment = 0; // attachment index
		colorAttachementRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachementRef;

		VkRenderPassCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &colorAttachment;
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpass;

		VkRenderPass renderPass = VK_NULL_HANDLE;
		VK_CHECK(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass));

		return renderPass;
	}


	std::vector<VkAttachmentDescription> GetAttachmentDescriptions(const std::vector<Attachment>& attachments, const std::vector<LoadStoreInfo>& loadStoreInfos)
	{
		assert(attachments.size() == loadStoreInfos.size());

		std::vector<VkAttachmentDescription> descriptions;

		for (size_t i = 0; i < attachments.size(); ++i)
		{
			VkAttachmentDescription desc{};

			desc.format = attachments[i].format;
			desc.samples = attachments[i].samples;
			desc.initialLayout = attachments[i].layout;
			desc.finalLayout = IsDepthStencilFormat(attachments[i].format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			// Load store actions
			// if (i < loadStoreInfos.size())
			{
				desc.loadOp = loadStoreInfos[i].loadOp;
				desc.storeOp = loadStoreInfos[i].storeOp;
				desc.stencilLoadOp = loadStoreInfos[i].loadOp;
				desc.stencilStoreOp = loadStoreInfos[i].storeOp;
			}

			descriptions.push_back(desc);
		}

		return descriptions;
	}

	void SetAttachmentLayouts(std::vector<VkSubpassDescription>& subpassDescriptions, std::vector<VkAttachmentDescription>& attachmentDescriptions)
	{
		// Make the initial layout same as in the first subpass using that attachment
		for (auto& subpassDesc : subpassDescriptions)
		{
			for (uint32_t i = 0; i < subpassDesc.colorAttachmentCount; ++i)
			{
				auto& ref = subpassDesc.pColorAttachments[i];
				// Set it only if not defined yet
				if (attachmentDescriptions[ref.attachment].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
					attachmentDescriptions[ref.attachment].initialLayout = ref.layout;
			}

			for (uint32_t i = 0; i < subpassDesc.inputAttachmentCount; ++i)
			{
				auto& ref = subpassDesc.pInputAttachments[i];
				if (attachmentDescriptions[ref.attachment].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
					attachmentDescriptions[ref.attachment].initialLayout = ref.layout;
			}

			if (subpassDesc.pDepthStencilAttachment)
			{
				auto& ref = *subpassDesc.pDepthStencilAttachment;
				if (attachmentDescriptions[ref.attachment].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
					attachmentDescriptions[ref.attachment].initialLayout = ref.layout;
			}

			if (subpassDesc.pResolveAttachments)
			{
				for (uint32_t i = 0; i < subpassDesc.colorAttachmentCount; ++i)
				{
					auto& ref = subpassDesc.pResolveAttachments[i];
					if (attachmentDescriptions[ref.attachment].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
						attachmentDescriptions[ref.attachment].initialLayout = ref.layout;
				}
			}

			// TODO: DepthStencilResolve
			// ...
		}

		// Make the final layout same as the last subpass layout
		{
			auto& subpassDesc = subpassDescriptions.back();

			for (uint32_t i = 0; i < subpassDesc.colorAttachmentCount; ++i)
			{
				auto& ref = subpassDesc.pColorAttachments[i];

				attachmentDescriptions[ref.attachment].finalLayout = ref.layout;
			}

			for (uint32_t i = 0; i < subpassDesc.inputAttachmentCount; ++i)
			{
				auto& ref = subpassDesc.pInputAttachments[i];

				attachmentDescriptions[ref.attachment].finalLayout = ref.layout;

				// Do not use depth attachment if used as input
				if (IsDepthStencilFormat(attachmentDescriptions[ref.attachment].format))
					subpassDesc.pDepthStencilAttachment = nullptr;
			}

			if (subpassDesc.pDepthStencilAttachment)
			{
				auto& ref = *subpassDesc.pDepthStencilAttachment;

				attachmentDescriptions[ref.attachment].finalLayout = ref.layout;
			}

			if (subpassDesc.pResolveAttachments)
			{
				for (uint32_t i = 0; i < subpassDesc.colorAttachmentCount; ++i)
				{
					auto& ref = subpassDesc.pResolveAttachments[i];

					attachmentDescriptions[ref.attachment].finalLayout = ref.layout;
				}
			}

			// TODO: DepthStencilResolve
			// ... 
		}
	}

	std::vector<VkSubpassDependency> GetSubpassDependencies(uint32_t subpassCount)
	{
		assert(subpassCount > 1);

		std::vector<VkSubpassDependency> dependencies(subpassCount - 1);
		for (uint32_t i = 0; i < subpassCount - 1; ++i)
		{
			auto& dependency = dependencies[i];
			dependency.srcSubpass = i;
			dependency.dstSubpass = i + 1;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		return dependencies;
	}

	/// Render pass

	void RenderPass::Init(const Device& device)
	{
		UpdateAttachments();

		const auto attachmentCount = static_cast<uint32_t>(attachments.size());

		auto attachmentDescriptions = GetAttachmentDescriptions(attachments, loadStoreInfos);

		// Store attachments for every subpass
		std::vector<std::vector<VkAttachmentReference>> inputAttachments{ subpassCount };
		std::vector<std::vector<VkAttachmentReference>> colorAttachments{ subpassCount };
		std::vector<std::vector<VkAttachmentReference>> depthStencilAttachments{ subpassCount };
		std::vector<std::vector<VkAttachmentReference>> colorResolveAttachments{ subpassCount };
		std::vector<std::vector<VkAttachmentReference>> depthResolveAttachments{ subpassCount };

		for (size_t i = 0; i < subpasses.size(); ++i)
		{
			auto& subpass = subpasses[i];

			// Fill input attachments references
			for (auto inputAttachment : subpass.inputAttachments)
			{
				auto defaultLayout = IsDepthStencilFormat(attachmentDescriptions[i].format) ?
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				auto initLayout = attachments[inputAttachment].layout == VK_IMAGE_LAYOUT_UNDEFINED ?
					defaultLayout : attachments[inputAttachment].layout;

				inputAttachments[i].push_back(VkAttachmentReference{ inputAttachment, initLayout });
			}

			// Fill color attachments references
			for (auto outputAttachment : subpass.outputAttachments)
			{
				auto initLayout = attachments[outputAttachment].layout == VK_IMAGE_LAYOUT_UNDEFINED ?
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : attachments[outputAttachment].layout;

				auto& desc = attachmentDescriptions[outputAttachment];
				if (!IsDepthStencilFormat(desc.format))
					colorAttachments[i].push_back(VkAttachmentReference{ outputAttachment, initLayout });
			}

			for (auto resolveAttachment : subpass.colorResolveAttachments)
			{
				auto initLayout = attachments[resolveAttachment].layout == VK_IMAGE_LAYOUT_UNDEFINED ?
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : attachments[resolveAttachment].layout;

				colorResolveAttachments[i].push_back(VkAttachmentReference{ resolveAttachment, initLayout });
			}

			if (subpass.bHasDepthStencilAttachment)
			{
				uint32_t dsAttachment = subpass.depthStencilResolveMode != VK_RESOLVE_MODE_NONE ?
					attachmentCount - 2 : attachmentCount - 1;
				auto initLayout = attachments[dsAttachment].layout == VK_IMAGE_LAYOUT_UNDEFINED ?
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : attachments[dsAttachment].layout;

				depthStencilAttachments[i].push_back(VkAttachmentReference{ dsAttachment, initLayout });

				if (subpass.depthStencilResolveMode != VK_RESOLVE_MODE_NONE)
				{
					uint32_t dsrAttachment = attachmentCount - 1;
					initLayout = attachments[dsrAttachment].layout == VK_IMAGE_LAYOUT_UNDEFINED ?
						VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : attachments[dsrAttachment].layout;

					depthResolveAttachments[i].push_back(VkAttachmentReference{ dsrAttachment, initLayout });
				}
			}
		}

		std::vector<VkSubpassDescription> subpassDescriptions;
		subpassDescriptions.reserve(subpassCount);
		VkSubpassDescriptionDepthStencilResolve depthResolve{};
		for (size_t i = 0; i < subpasses.size(); ++i)
		{
			const auto& subpass = subpasses[i];

			VkSubpassDescription desc{};
			desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

			desc.pInputAttachments = inputAttachments[i].empty() ? nullptr : inputAttachments[i].data();
			desc.inputAttachmentCount = static_cast<uint32_t>(inputAttachments[i].size());

			desc.pColorAttachments = colorAttachments[i].empty() ? nullptr : colorAttachments[i].data();
			desc.colorAttachmentCount = static_cast<uint32_t>(colorAttachments[i].size());

			desc.pResolveAttachments = colorResolveAttachments[i].empty() ? nullptr : colorResolveAttachments[i].data();

			desc.pDepthStencilAttachment = nullptr;
			if (!depthStencilAttachments[i].empty())
			{
				desc.pDepthStencilAttachment = depthStencilAttachments[i].data();

				if (!depthResolveAttachments[i].empty())
				{
					// If the pNext list of VkSubpassDescription2 includes a VkSubpassDescriptionDepthStencilResolve structure,
					// then that structure describes multisample resolve operations for the depth/stencil attachment in a subpass
					depthResolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
					depthResolve.depthResolveMode = subpass.depthStencilResolveMode;

					// TODO:
					// depthResolve.pDepthStencilResolveAttachment = &depthResolveAttachments[i][0];
					// desc.pNext = &depthResolve;

					auto& ref = depthResolveAttachments[i][0];
					if (attachmentDescriptions[ref.attachment].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
						attachmentDescriptions[ref.attachment].initialLayout = ref.layout;
				}
			}

			subpassDescriptions.push_back(desc);
		}

		// Default subpass
		if (subpasses.empty())
		{
			VkSubpassDescription desc{};
			desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

			uint32_t defaultDepthStencilAttachment{ VK_ATTACHMENT_UNUSED };

			for (uint32_t i = 0; i < attachmentCount; ++i)
			{
				if (IsDepthStencilFormat(attachmentDescriptions[i].format))
				{
					if (defaultDepthStencilAttachment == VK_ATTACHMENT_UNUSED)
						defaultDepthStencilAttachment = i;
					continue;
				}
				// TODO: Why Vulkan-Samples use layout `VK_IMAGE_LAYOUT_GENERAL` ?
				colorAttachments[0].push_back(VkAttachmentReference{ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // VK_IMAGE_LAYOUT_UNDEFINED VK_IMAGE_LAYOUT_GENERAL
			}

			desc.pColorAttachments = colorAttachments[0].data();
			desc.colorAttachmentCount = static_cast<uint32_t>(colorAttachments[0].size());

			if (defaultDepthStencilAttachment != VK_ATTACHMENT_UNUSED)
			{
				depthStencilAttachments[0].push_back(VkAttachmentReference{ defaultDepthStencilAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

				desc.pDepthStencilAttachment = depthStencilAttachments[0].data();
			}

			subpassDescriptions.push_back(desc);
		}

		subpassCount = static_cast<uint32_t>(subpassDescriptions.size());

		SetAttachmentLayouts(subpassDescriptions, attachmentDescriptions);

		colorOutputCounts.reserve(subpassCount);
		for (uint32_t i = 0; i < subpassCount; ++i)
			colorOutputCounts.push_back(static_cast<uint32_t>(colorAttachments[i].size()));

		VkRenderPassCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = attachmentCount;
		createInfo.pAttachments = attachmentDescriptions.data();
		createInfo.subpassCount = subpassCount;
		createInfo.pSubpasses = subpassDescriptions.data();
		if (subpassCount > 1)
		{
			const auto& subpassDependencies = GetSubpassDependencies(subpassCount);
			createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
			createInfo.pDependencies = subpassDependencies.data();
		}

		VK_CHECK(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass));
		assert(renderPass);
	}

	void RenderPass::Destroy(const Device& device)
	{
		if (renderPass != VK_NULL_HANDLE)
		{
			vkDestroyRenderPass(device, renderPass, nullptr);
			renderPass = VK_NULL_HANDLE;
		}

		attachments.clear();
		loadStoreInfos.clear();
		subpasses.clear();
	}

	// TODO: no resolving yet
	void RenderPass::UpdateAttachments()
	{
		attachments.clear();
		loadStoreInfos.clear();

		if (activeColorAttachmentCount > 0)
		{
			attachments.resize(activeColorAttachmentCount);
			loadStoreInfos.resize(activeColorAttachmentCount);

			std::copy_n(colorAttachments.begin(), activeColorAttachmentCount, attachments.begin());
			std::copy_n(colorLoadStoreInfos.begin(), activeColorAttachmentCount, loadStoreInfos.begin());
		}

		if (depthAttachment.format != VK_FORMAT_UNDEFINED)
		{
			attachments.push_back(depthAttachment);
			loadStoreInfos.push_back(depthLoadStoreInfo);
		}
	}
}
