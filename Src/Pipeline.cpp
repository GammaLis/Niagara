#include "Pipeline.h"
#include "Device.h"

namespace Niagara
{
	void Pipeline::GatherDescriptors()
	{
		auto shaders = GetPipelineShaders();

		descriptorCount = 0;
		descriptorMask = 0;
		for (const auto& shader : shaders)
		{
			if (shader->resourceMask)
			{
				for (uint32_t i = 0; i < s_MaxDescriptorNum; ++i)
				{
					if (shader->resourceMask & (1 << i))
					{
						if (descriptorMask & (1 << i))
							assert(descriptorTypes[i] == shader->resourceTypes[i]);
						else
						{
							descriptorTypes[i] = shader->resourceTypes[i];
							descriptorMask |= 1 << i;
						}
						++descriptorCount;
					}
				}
			}
		}

		descriptorStart = descriptorEnd = 0;
		for (uint32_t i = 0; i < s_MaxDescriptorNum; ++i)
		{
			if (descriptorMask & (1 << i))
			{
				if (descriptorEnd == 0) descriptorStart = i;
				descriptorEnd = i + 1;
			}
		}
	}

	std::vector<VkPipelineShaderStageCreateInfo> Pipeline::GetShaderStagesCreateInfo() const
	{
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo;

		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.pName = "main";

		auto shaders = GetPipelineShaders();
		for (const auto& shader : shaders)
		{
			if (shader->module != VK_NULL_HANDLE)
			{
				shaderStageCreateInfo.module = shader->module;
				shaderStageCreateInfo.stage = shader->stage;

				shaderStagesCreateInfo.push_back(shaderStageCreateInfo);
			}
		}

		return shaderStagesCreateInfo;
	}

	VkDescriptorSetLayout Pipeline::CreateDescriptorSetLayout(VkDevice device, bool pushDescriptorsSupported) const
	{
		auto shaders = GetPipelineShaders();

		std::vector<VkDescriptorSetLayoutBinding> setBindings = Shader::GetSetBindings(shaders, descriptorTypes, descriptorMask);

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pBindings = setBindings.data();
		createInfo.bindingCount = static_cast<uint32_t>(setBindings.size());
		createInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;

		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayout));

		return setLayout;
	}

	VkDescriptorUpdateTemplate Pipeline::CreateDescriptorUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, bool pushDescriptorsSupported) const
	{
		auto shaders = GetPipelineShaders();

		std::vector<VkDescriptorUpdateTemplateEntry> entries = Shader::GetUpdateTemplateEntries(shaders);

		VkDescriptorUpdateTemplateCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
		createInfo.pDescriptorUpdateEntries = entries.data();
		createInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entries.size());
		createInfo.templateType = pushDescriptorsSupported ? VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR : VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
		createInfo.descriptorSetLayout = pushDescriptorsSupported ? nullptr : descriptorSetLayout;
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
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // Optional

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		return pipelineLayout;
	}

	void Pipeline::Init(VkDevice device)
	{
		assert(ShadersValid());

		GatherDescriptors();

		auto descriptorSetLayout = CreateDescriptorSetLayout(device, g_PushDescriptorsSupported);
		assert(descriptorSetLayout);
		this->descriptorSetLayout = descriptorSetLayout;

		auto pipelineLayout = CreatePipelineLayout(device, g_PushDescriptorsSupported);
		assert(pipelineLayout);
		this->layout = pipelineLayout;
	}

	void Pipeline::Destroy(VkDevice device)
	{
		if (descriptorSetLayout)
		{
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			descriptorSetLayout = VK_NULL_HANDLE;
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

		auto descriptorUpdateTemplate = CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_GRAPHICS, g_PushDescriptorsSupported);
		assert(descriptorUpdateTemplate);
		this->descriptorUpdateTemplate = descriptorUpdateTemplate;

		auto shaderStages = GetShaderStagesCreateInfo();

		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.pStages = shaderStages.data();
		createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		createInfo.pVertexInputState = &pipelineState.vertexInputState; // <--
		createInfo.pInputAssemblyState = &pipelineState.inputAssemblyState; // <--
		createInfo.pViewportState = &pipelineState.viewportState;
		createInfo.pRasterizationState = &pipelineState.rasterizationState;
		createInfo.pMultisampleState = &pipelineState.multisampleState;
		createInfo.pDepthStencilState = &pipelineState.depthStencilState; // <--
		createInfo.pColorBlendState = &pipelineState.colorBlendState; // <--
		createInfo.pDynamicState = &pipelineState.dynamicState;
		createInfo.layout = layout;
		createInfo.renderPass = renderPass;
		createInfo.subpass = subpass;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		pipeline = VK_NULL_HANDLE;
		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));
	}

	
	void GraphicsPipeline::Destroy(VkDevice device)
	{
		vertShader.Cleanup(device);
		taskShader.Cleanup(device);
		meshShader.Cleanup(device);
		fragShader.Cleanup(device);

		Pipeline::Destroy(device);
	}


	void ComputePipeline::Init(VkDevice device)
	{
		Pipeline::Init(device);

		auto descriptorUpdateTemplate = CreateDescriptorUpdateTemplate(device, VK_PIPELINE_BIND_POINT_COMPUTE, g_PushDescriptorsSupported);
		assert(descriptorUpdateTemplate);
		this->descriptorUpdateTemplate = descriptorUpdateTemplate;
	}

	/// Compute pipeline

	void ComputePipeline::Destroy(VkDevice device)
	{
		computeShader.Cleanup(device);

		Pipeline::Destroy(device);
	}
	
}
