#include "Pipeline.h"
#include "Device.h"
#include "RenderPass.h"

namespace Niagara
{
	void Pipeline::GatherDescriptors()
	{
		auto shaders = GetPipelineShaders();
		assert(!shaders.empty());

		auto &descriptorSetInfo = descriptorSetInfos[0];

		descriptorSetInfo.start = s_MaxDescriptorNum;
		descriptorSetInfo.count = 0;
		descriptorSetInfo.mask = 0;
		uint32_t setEnd = 0;
		for (const auto& shader : shaders)
		{
			if (shader->resourceMask)
			{
				for (uint32_t i = 0; i < s_MaxDescriptorNum; ++i)
				{
					if (shader->resourceMask & (1 << i))
					{
						if (descriptorSetInfo.start == s_MaxDescriptorNum)
							descriptorSetInfo.start = i;
						setEnd = i + 1;

						if (descriptorSetInfo.mask & (1 << i))
							assert(descriptorSetInfo.types[i] == shader->resourceTypes[i]);
						else
						{
							descriptorSetInfo.types[i] = shader->resourceTypes[i];
							descriptorSetInfo.mask |= 1 << i;
						}
					}
				}
			}
		}

		if (setEnd > 0)
			descriptorSetInfo.count = setEnd - descriptorSetInfo.start;
	}

	void Pipeline::SpirvCrossGatherDescriptors()
	{
		pushConstants.clear();
		shaderResourceMap.clear();
		setResources.clear();

		// Collect and combine all the shader resources from each of the shader modules
		auto shaders = GetPipelineShaders();
		assert(!shaders.empty());

		for (const auto &shader : shaders)
		{
			if (!shader->IsValid()) continue;
			
			for (const auto &shaderResource : shader->resources)
			{
				if (shaderResource.type == ShaderResourceType::Input || 
					shaderResource.type == ShaderResourceType::InputAttachment || 
					shaderResource.type == ShaderResourceType::Output ||
					shaderResource.type == ShaderResourceType::SpecializationConstant) // No binding point, not used yet
					continue;

				// Push constants
				if (shaderResource.type == ShaderResourceType::PushConstant) // No binding point
				{
					auto it = pushConstants.find(shaderResource.name);

					if (it != pushConstants.end())
						it->second.stages |= shaderResource.stages;
					else
						pushConstants.emplace(shaderResource.name, shaderResource);

					continue;
				}

				uint8_t key = (shaderResource.set << 6u) | shaderResource.binding;

				auto it = shaderResourceMap.find(key);

				if (it != shaderResourceMap.end())
					it->second.stages |= shaderResource.stages;
				else
					shaderResourceMap.emplace(key, shaderResource);
			}
		}

		// Separate them into their respective sets
		for (const auto &kvp : shaderResourceMap)
		{
			const auto &shaderResource = kvp.second;

			auto it = setResources.find(shaderResource.set);
			if (it != setResources.end())
				it->second.push_back(shaderResource);
			else
				setResources.emplace(shaderResource.set, std::vector<ShaderResource>{ shaderResource });			
		}

		bUsePushConstants = !pushConstants.empty();
	}

	std::vector<VkPipelineShaderStageCreateInfo> Pipeline::GetShaderStagesCreateInfo() const
	{
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo;

		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		
		auto shaders = GetPipelineShaders();
		assert(!shaders.empty());

		for (const auto& shader : shaders)
		{
			if (shader->IsValid())
			{
				shaderStageCreateInfo.module = shader->module;
				shaderStageCreateInfo.stage = shader->stage;
				shaderStageCreateInfo.pName = shader->entryPoint.c_str();

				shaderStagesCreateInfo.push_back(shaderStageCreateInfo);
			}
		}

		return shaderStagesCreateInfo;
	}

	std::vector<VkDescriptorSetLayoutBinding> Pipeline::GetDescriptorBindings(uint32_t set) const
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

		const auto it = setResources.find(set);

		if (it == setResources.end())
			return setLayoutBindings;

		for (const auto& resource : it->second)
		{
			VkDescriptorSetLayoutBinding setBinding{};
			setBinding.binding = resource.binding;
			setBinding.descriptorType = GetDescriptorType(resource.type);
			setBinding.descriptorCount = resource.arraySize;
			setBinding.stageFlags = resource.stages;

			setLayoutBindings.push_back(setBinding);
		}

		return setLayoutBindings;
	}

	std::vector<VkDescriptorUpdateTemplateEntry> Pipeline::GetDescriptorUpdateTemplateEntries(uint32_t set) const
	{
		std::vector<VkDescriptorUpdateTemplateEntry> entries;

		const auto it = setResources.find(set);

		if (it == setResources.end())
			return entries;

		uint32_t bindingStart = 0xFF;
		for (const auto& resource : it->second)
			bindingStart = std::min(bindingStart, resource.binding);

		for (const auto& resource : it->second)
		{
			VkDescriptorUpdateTemplateEntry entry{};
			entry.dstBinding = resource.binding;
			entry.descriptorType = GetDescriptorType(resource.type);
			entry.descriptorCount = resource.arraySize;
			entry.dstArrayElement = 0;
			entry.stride = sizeof(DescriptorInfo);
			entry.offset = entry.stride * (resource.binding - bindingStart);

			entries.push_back(entry);
		}

		return entries;
	}

	VkDescriptorSetLayout Pipeline::CreateDescriptorSetLayout(VkDevice device, bool pushDescriptorsSupported) const
	{
		auto shaders = GetPipelineShaders();
		assert(!shaders.empty());

		std::vector<VkDescriptorSetLayoutBinding> setBindings = Shader::GetSetBindings(shaders, descriptorSetInfos[0].types, descriptorSetInfos[0].mask);

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pBindings = setBindings.data();
		createInfo.bindingCount = static_cast<uint32_t>(setBindings.size());
		createInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;

		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayout));

		return setLayout;
	}

	std::vector<VkDescriptorSetLayout> Pipeline::CreateDescriptorSetLayouts(VkDevice device, bool pushDescriptorsSupported) const
	{
		std::vector<VkDescriptorSetLayout> setLayouts;

		if (setResources.empty())
			return setLayouts;

		setLayouts.resize(setResources.size());

		for (const auto &kvp : setResources)
		{
			auto setLayoutBindings = GetDescriptorBindings(kvp.first);

			VkDescriptorSetLayoutCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			createInfo.pBindings = setLayoutBindings.data();
			createInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
			// ERROR::vkCreatePipelineLayout() Multiple push descriptor sets found. 
			// The Vulkan spec states: pSetLayouts must not contain more than one descriptor set layout that was created with 
			// VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR set 
			// (https://vulkan.lunarg.com/doc/view/1.3.236.0/windows/1.3-extensions/vkspec.html#VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00293)
			createInfo.flags = pushDescriptorsSupported && kvp.first == 0 ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;

			VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
			VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayout));
			assert(setLayout);

			setLayouts[kvp.first] = setLayout;
		}

		return setLayouts;
	}

	VkDescriptorUpdateTemplate Pipeline::CreateDescriptorUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, uint32_t setLayoutIndex, bool pushDescriptorsSupported) const
	{
		std::vector<VkDescriptorUpdateTemplateEntry> entries = GetDescriptorUpdateTemplateEntries(setLayoutIndex);

		VkDescriptorUpdateTemplateCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
		createInfo.pDescriptorUpdateEntries = entries.data();
		createInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entries.size());
		createInfo.templateType = pushDescriptorsSupported ? VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR : VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
		createInfo.descriptorSetLayout = pushDescriptorsSupported ? nullptr : descriptorSetLayouts.at(setLayoutIndex);
		createInfo.pipelineLayout = layout;
		createInfo.pipelineBindPoint = bindPoint;

		VkDescriptorUpdateTemplate updateTemplate = VK_NULL_HANDLE;
		VK_CHECK(vkCreateDescriptorUpdateTemplate(device, &createInfo, nullptr, &updateTemplate));

		return updateTemplate;
	}

	std::vector<VkPushConstantRange> Pipeline::CreatePushConstantRanges() const
	{
		std::vector<VkPushConstantRange> ranges;

		VkPushConstantRange range{};
		for (const auto &kvp : pushConstants)
		{
			const auto &pushConstant = kvp.second;
			range.stageFlags = pushConstant.stages;
			range.offset = pushConstant.offset;
			range.size = pushConstant.size;

			ranges.push_back(range);
		}

		return ranges;
	}

	VkPipelineLayout Pipeline::CreatePipelineLayout(VkDevice device, bool pushDescriptorSupported) const
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
		// Push constants
		if (!pushConstantRanges.empty())
		{
			pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
			pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		}

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		return pipelineLayout;
	}

	void GraphicsPipelineState::Update()
	{
		// Vertex input state
		vertexInputState.pVertexBindingDescriptions = bindingDescriptions.data();
		vertexInputState.vertexBindingDescriptionCount= static_cast<uint32_t>(bindingDescriptions.size());
		vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());

		// Viewport state
		viewportState.pViewports = viewports.data();
		viewportState.viewportCount = std::max(1u, static_cast<uint32_t>(viewports.size()));
		viewportState.pScissors = scissors.data();
		viewportState.scissorCount = std::max(1u, static_cast<uint32_t>(scissors.size()));

		// Color blend state
		colorBlendState.pAttachments = colorBlendAttachments.data();
		colorBlendState.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());

		// Dynamic state
		dynamicState.pDynamicStates = dynamicStates.data();
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	}


	/// Pipeline

	void Pipeline::UpdateDescriptorSetInfo(DescriptorSetInfo& setInfo, uint32_t set) const
	{
		auto it = setResources.find(set);

		if (it == setResources.end()) return;

		const auto& shaderResources = it->second;
		setInfo.mask = 0;
		setInfo.start = s_MaxDescriptorNum;
		uint32_t setEnd = 0;
		for (const auto& resource : shaderResources)
		{
			setInfo.start = std::min(setInfo.start, resource.binding);
			setEnd = std::max(resource.binding + 1, setEnd);

			setInfo.mask |= (1 << resource.binding);
			setInfo.types[resource.binding] = GetDescriptorType(resource.type);
		}

		setInfo.count = setEnd - setInfo.start;
	}

	void Pipeline::Init(VkDevice device)
	{
		assert(ShadersValid());

		Destroy(device);

#if USE_SPIRV_CROSS
		SpirvCrossGatherDescriptors();

		this->descriptorSetLayouts = CreateDescriptorSetLayouts(device, g_PushDescriptorsSupported);

		if (bUsePushConstants)
			this->pushConstantRanges = CreatePushConstantRanges();

#else
		GatherDescriptors();

		auto&& descriptorSetLayout = CreateDescriptorSetLayout(device, g_PushDescriptorsSupported);
		this->descriptorSetLayouts.emplace_back(descriptorSetLayout);
#endif
		
		auto pipelineLayout = CreatePipelineLayout(device, g_PushDescriptorsSupported);
		assert(pipelineLayout);
		this->layout = pipelineLayout;

		shaderStagesInfo = GetShaderStagesCreateInfo();
		assert(!shaderStagesInfo.empty());
	}

	void Pipeline::Destroy(VkDevice device)
	{
		if (!descriptorSetLayouts.empty())
		{
			for (const auto &descriptorSetLayout : descriptorSetLayouts)
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

			descriptorSetLayouts.clear();
		}

		if (descriptorUpdateTemplate)
		{
			vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, nullptr);
			descriptorUpdateTemplate = VK_NULL_HANDLE;
		}

		if (layout) 
		{
			vkDestroyPipelineLayout(device, layout, nullptr);
			layout = VK_NULL_HANDLE;
		}
		if (pipeline)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			pipeline = VK_NULL_HANDLE;
		}
	}


	/// Graphics pipeline

	void GraphicsPipeline::Init(VkDevice device)
	{
		assert(renderPass != nullptr || (!colorAttachmentFormats.empty() || depthAttachmentFormat != VK_FORMAT_UNDEFINED));

		Pipeline::Init(device);

		if (!setResources.empty())
			descriptorUpdateTemplate = CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, g_PushDescriptorsSupported);
		else
			descriptorUpdateTemplate = VK_NULL_HANDLE;

		pipelineState.Update();

		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.pStages = shaderStagesInfo.data();
		createInfo.stageCount = static_cast<uint32_t>(shaderStagesInfo.size());
		createInfo.pVertexInputState = &pipelineState.vertexInputState; // <--
		createInfo.pInputAssemblyState = &pipelineState.inputAssemblyState;
		createInfo.pViewportState = &pipelineState.viewportState; // <--
		createInfo.pRasterizationState = &pipelineState.rasterizationState;
		createInfo.pMultisampleState = &pipelineState.multisampleState;
		createInfo.pDepthStencilState = &pipelineState.depthStencilState; // <--
		createInfo.pColorBlendState = &pipelineState.colorBlendState; // <--
		createInfo.pDynamicState = &pipelineState.dynamicState; // <--
		createInfo.layout = layout;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		VkPipelineRenderingCreateInfo renderingCreateInfo{};
		// Render pass
		if (renderPass != nullptr)
		{
			createInfo.renderPass = renderPass->renderPass; // <--
			createInfo.subpass = subpass; // <--
		}
		else
		{
			// No render pass, use dynamic rendering
			renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
			renderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
			renderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
			renderingCreateInfo.depthAttachmentFormat = depthAttachmentFormat;
			renderingCreateInfo.stencilAttachmentFormat = depthAttachmentFormat;

			createInfo.pNext = &renderingCreateInfo;

			createInfo.renderPass = VK_NULL_HANDLE;
			createInfo.subpass = 0;
		}

		pipeline = VK_NULL_HANDLE;
		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));
		assert(pipeline);
	}

	void GraphicsPipeline::Destroy(VkDevice device)
	{
		Pipeline::Destroy(device);
	}

	void GraphicsPipeline::SetAttachments(const VkFormat* pColorAttachmentFormats, uint32_t colorAttachmentCount, VkFormat depthAttachmentFormat)
	{
		if (colorAttachmentCount > 0)
		{
			colorAttachmentFormats.resize(colorAttachmentCount);
			std::copy_n(pColorAttachmentFormats, colorAttachmentCount, colorAttachmentFormats.data());
		}
		else
		{
			colorAttachmentFormats.clear();
		}

		this->depthAttachmentFormat = depthAttachmentFormat;
	}

	std::vector<const Shader*> GraphicsPipeline::GetPipelineShaders() const
	{
		std::vector<const Shader*> shaders;

		if (vertShader) 
			shaders.push_back(vertShader);
		if (fragShader) 
			shaders.push_back(fragShader);
		if (taskShader) 
			shaders.push_back(taskShader);
		if (meshShader) 
			shaders.push_back(meshShader);

		return shaders;
	}


	/// Compute pipeline

	void ComputePipeline::Init(VkDevice device)
	{
		Pipeline::Init(device);

		if (!setResources.empty())
			descriptorUpdateTemplate = CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_COMPUTE, 0, g_PushDescriptorsSupported);
		else
			descriptorUpdateTemplate = VK_NULL_HANDLE;
		
		VkComputePipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		createInfo.stage = shaderStagesInfo[0];
		createInfo.layout = layout;
		
		pipeline = VK_NULL_HANDLE;
		VK_CHECK(vkCreateComputePipelines(device, pipelineCache, 1, &createInfo, nullptr, &pipeline));
		assert(pipeline);
	}

	void ComputePipeline::Destroy(VkDevice device)
	{
		Pipeline::Destroy(device);
	}
	
}
