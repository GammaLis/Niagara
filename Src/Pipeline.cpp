#include "Pipeline.h"
#include "Device.h"

namespace Niagara
{
	void Pipeline::GatherDescriptors()
	{
		auto shaders = GetPipelineShaders();

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

	void Pipeline::NewGatherDescriptors()
	{
		// Collect and combine all the shader resources from each of the shader modules
		auto shaders = GetPipelineShaders();
		for (const auto &shader : shaders)
		{
			if (!shader->IsValid()) continue;
			
			for (const auto &shaderResource : shader->resources)
			{
				if (shaderResource.type == ShaderResourceType::Input || 
					shaderResource.type == ShaderResourceType::InputAttachment || 
					shaderResource.type == ShaderResourceType::Output ||
					shaderResource.type == ShaderResourceType::PushConstant || // No binding point
					shaderResource.type == ShaderResourceType::SpecializationConstant) // No binding point
					continue;

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
	}

	std::vector<VkPipelineShaderStageCreateInfo> Pipeline::GetShaderStagesCreateInfo() const
	{
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo;

		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		
		auto shaders = GetPipelineShaders();
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

		for (const auto& resource : it->second)
		{
			VkDescriptorUpdateTemplateEntry entry{};
			entry.dstBinding = resource.binding;
			entry.descriptorType = GetDescriptorType(resource.type);
			entry.descriptorCount = resource.arraySize;
			entry.dstArrayElement = 0;
			entry.stride = sizeof(DescriptorInfo);
			entry.offset = entry.stride * resource.binding;

			entries.push_back(entry);
		}

		return entries;
	}

	VkDescriptorSetLayout Pipeline::CreateDescriptorSetLayout(VkDevice device, bool pushDescriptorsSupported) const
	{
		auto shaders = GetPipelineShaders();

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
		std::vector<VkDescriptorSetLayout> setLayouts(setResources.size());

		for (const auto &kvp : setResources)
		{
			auto setLayoutBindings = GetDescriptorBindings(kvp.first);

			VkDescriptorSetLayoutCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			createInfo.pBindings = setLayoutBindings.data();
			createInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
			createInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;

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

	VkPipelineLayout Pipeline::CreatePipelineLayout(VkDevice device, bool pushDescriptorSupported) const
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // Optional

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

#if USE_SPIRV_CROSS
		NewGatherDescriptors();

		this->descriptorSetLayouts = CreateDescriptorSetLayouts(device, g_PushDescriptorsSupported);

#else
		GatherDescriptors();

		auto&& descriptorSetLayout = CreateDescriptorSetLayout(device, g_PushDescriptorsSupported);
		this->descriptorSetLayouts.emplace_back(descriptorSetLayout);
#endif
		
		auto pipelineLayout = CreatePipelineLayout(device, g_PushDescriptorsSupported);
		assert(pipelineLayout);
		this->layout = pipelineLayout;
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
		if (renderPass)
		{
			vkDestroyRenderPass(device, renderPass, nullptr);
			renderPass = VK_NULL_HANDLE;
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
		Pipeline::Init(device);

		auto descriptorUpdateTemplate = CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, g_PushDescriptorsSupported);
		assert(descriptorUpdateTemplate);
		this->descriptorUpdateTemplate = descriptorUpdateTemplate;

		auto shaderStages = GetShaderStagesCreateInfo();

		pipelineState.Update();

		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.pStages = shaderStages.data();
		createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		createInfo.pVertexInputState = &pipelineState.vertexInputState; // <--
		createInfo.pInputAssemblyState = &pipelineState.inputAssemblyState;
		createInfo.pViewportState = &pipelineState.viewportState; // <--
		createInfo.pRasterizationState = &pipelineState.rasterizationState;
		createInfo.pMultisampleState = &pipelineState.multisampleState;
		createInfo.pDepthStencilState = &pipelineState.depthStencilState; // <--
		createInfo.pColorBlendState = &pipelineState.colorBlendState; // <--
		createInfo.pDynamicState = &pipelineState.dynamicState; // <--
		createInfo.layout = layout;
		createInfo.renderPass = renderPass; // <--
		createInfo.subpass = subpass; // <--
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		pipeline = VK_NULL_HANDLE;
		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));
		assert(pipeline);
	}

	void GraphicsPipeline::Destroy(VkDevice device)
	{
		vertShader.Cleanup(device);
		taskShader.Cleanup(device);
		meshShader.Cleanup(device);
		fragShader.Cleanup(device);

		Pipeline::Destroy(device);
	}


	/// Compute pipeline

	void ComputePipeline::Init(VkDevice device)
	{
		Pipeline::Init(device);

		auto descriptorUpdateTemplate = CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_COMPUTE, 0, g_PushDescriptorsSupported);
		assert(descriptorUpdateTemplate);
		this->descriptorUpdateTemplate = descriptorUpdateTemplate;

		// TODO...
	}

	void ComputePipeline::Destroy(VkDevice device)
	{
		computeShader.Cleanup(device);

		Pipeline::Destroy(device);
	}
	
}
