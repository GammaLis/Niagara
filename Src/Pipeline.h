#pragma once

#include "pch.h"
#include "Shaders.h"
#include <unordered_map>

// Ref: nvpro-core

namespace Niagara
{
	// Prints stats of the pipeline using VK_KHR_pipeline_executable_properties (don't forget to enable extension and set VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR)
	void PrintPipelineStats(VkDevice device, VkPipeline pipeline, const char *name, bool bVerbose = false);

	struct VertexInputState : VkPipelineVertexInputStateCreateInfo
	{
		VertexInputState() : VkPipelineVertexInputStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		}
		explicit VertexInputState(const VkPipelineVertexInputStateCreateInfo &o) : VkPipelineVertexInputStateCreateInfo(o) {  }
	};

	struct InputAssemblyState : VkPipelineInputAssemblyStateCreateInfo
	{
		explicit InputAssemblyState(VkPrimitiveTopology inTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) : VkPipelineInputAssemblyStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			topology = inTopology;
		}
		explicit InputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& o) : VkPipelineInputAssemblyStateCreateInfo(o) {  }
	};

	struct ViewportState : VkPipelineViewportStateCreateInfo
	{
		ViewportState() : VkPipelineViewportStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		}
		explicit ViewportState(const VkPipelineViewportStateCreateInfo& o) : VkPipelineViewportStateCreateInfo(o) {  }
	};

	struct RasterizationState : VkPipelineRasterizationStateCreateInfo
	{
		explicit RasterizationState(VkPolygonMode inPolygonMode = VK_POLYGON_MODE_FILL, VkCullModeFlagBits inCullMode = VK_CULL_MODE_BACK_BIT, VkFrontFace inFrontFace = VK_FRONT_FACE_CLOCKWISE, float inLineWidth = 1.0f)
			: VkPipelineRasterizationStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			polygonMode = inPolygonMode;
			cullMode = inCullMode;
			frontFace = inFrontFace;
			lineWidth = 1.0f;
		}
		explicit RasterizationState(const VkPipelineRasterizationStateCreateInfo& o) : VkPipelineRasterizationStateCreateInfo(o) {  }
	};

	struct MultisampleState : VkPipelineMultisampleStateCreateInfo
	{
		MultisampleState() : VkPipelineMultisampleStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		}
		explicit MultisampleState(const VkPipelineMultisampleStateCreateInfo& o) : VkPipelineMultisampleStateCreateInfo(o) {  }
	};

	struct DepthStencilState : VkPipelineDepthStencilStateCreateInfo
	{
		explicit DepthStencilState(VkBool32 inEenableDepthTest = VK_FALSE, VkBool32 inDepthWriteEnable = VK_FALSE, VkCompareOp inDepthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL) : VkPipelineDepthStencilStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthTestEnable = inEenableDepthTest;
			depthWriteEnable = inDepthWriteEnable;
			depthCompareOp = inDepthCompareOp;
		}
		explicit DepthStencilState(const VkPipelineDepthStencilStateCreateInfo &o) : VkPipelineDepthStencilStateCreateInfo(o) {  }
	};

	struct ColorBlendAttachmentState : VkPipelineColorBlendAttachmentState
	{
		explicit ColorBlendAttachmentState(VkBool32 inBlendEnable = VK_FALSE, VkColorComponentFlags inColorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
			: VkPipelineColorBlendAttachmentState{}
		{
			blendEnable = inBlendEnable;
			srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendOp = VK_BLEND_OP_ADD;
			srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			alphaBlendOp = VK_BLEND_OP_ADD;
			colorWriteMask = inColorWriteMask;
		}
		explicit ColorBlendAttachmentState(const VkPipelineColorBlendAttachmentState &o) : VkPipelineColorBlendAttachmentState(o) {  }
	};

	struct ColorBlendState : VkPipelineColorBlendStateCreateInfo
	{
		explicit ColorBlendState(const VkPipelineColorBlendAttachmentState *attachments = nullptr, uint32_t size = 0) : VkPipelineColorBlendStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			logicOp = VK_LOGIC_OP_COPY;
			pAttachments = attachments;
			attachmentCount = size;
		}
		explicit ColorBlendState(const VkPipelineColorBlendStateCreateInfo &o) : VkPipelineColorBlendStateCreateInfo(o) {  }
	};

	struct DynamicState : VkPipelineDynamicStateCreateInfo
	{
		DynamicState() : VkPipelineDynamicStateCreateInfo{}
		{
			sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		}
		DynamicState(const VkPipelineDynamicStateCreateInfo &o) : VkPipelineDynamicStateCreateInfo(o) {  }
	};

	// Helper class to create specialization constants for a Vulkan pipeline. The state tracks a pipeline globally, and not per shader.
	struct SpecializationConstantState
	{
	public:
		SpecializationConstantState() = default;

		void Reset() 
		{
			if (dirty)
				constantMap.clear();

			dirty = false;
		}

		bool IsDirty() const { return dirty; }
		void ClearDirty() { dirty = false; }

		void SetConstant(uint32_t constantId, uint32_t value)
		{
			auto it = constantMap.find(constantId);
			if (it != constantMap.end() && it->second == value)
				return;

			constantMap[constantId] = value;
			dirty = true;
		}

		void SetConstants(const std::vector<uint32_t>& values) 
		{
			for (uint32_t i = 0, imax = static_cast<uint32_t>(values.size()); i < imax; ++i)
				SetConstant(i, values[i]);
		}

		void SetConstants(const std::unordered_map<uint32_t, uint32_t> &values)
		{
			for (const auto& kvp : values)
				SetConstant(kvp.first, kvp.second);
		}

		void ResetConstants(const std::unordered_map<uint32_t, uint32_t>& values)
		{
			if (constantMap != values)
			{
				constantMap = values;
				dirty = true;
			}
		}

		std::unordered_map<uint32_t, uint32_t> constantMap;

	private:
		bool dirty{ false };
	};


	/**
	 * Most graphics pipelines have similar states, therefore the helper "GraphicsPipelineState" holds all the elements and initialize the structures
	 * with the proper default values, such as the primitive type, `PipelineColorBlendAttachmentState` with their mask, `DynamicState` for viewport
	 * and scissors, adjust depth test if enabled, line width to 1 pixel, for example.
	 */
	struct GraphicsPipelineState
	{
		GraphicsPipelineState() = default;

		void Update();

		VertexInputState vertexInputState;
		InputAssemblyState inputAssemblyState;
		ViewportState viewportState;
		RasterizationState rasterizationState;
		MultisampleState multisampleState;
		DepthStencilState depthStencilState;
		ColorBlendState colorBlendState;
		DynamicState dynamicState;

		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

		std::vector<ColorBlendAttachmentState> colorBlendAttachments = { ColorBlendAttachmentState() };
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		std::vector<VkViewport> viewports;
		std::vector<VkRect2D> scissors;
	};


	class Device;
	class RenderPass;

	class Pipeline
	{
	public:
		static constexpr uint32_t s_MaxDescrptorSetNum = 4;
		static constexpr uint32_t s_MaxDescriptorNum = 32;

		Pipeline() = default;
		virtual ~Pipeline() {  }

		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipelineCache pipelineCache = VK_NULL_HANDLE;

		const RenderPass* renderPass{ nullptr };
		uint32_t subpass = 0;

		// Stages
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesInfo;

		// Descriptors
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
		VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;

		// Push constants
		bool bUsePushConstants{ false };
		std::vector<VkPushConstantRange> pushConstantRanges;

		// Specialization constants
		bool bUseSpecializationConstants{ false };
		std::vector<VkSpecializationMapEntry> specializationMapEntries;
		std::vector<uint32_t> specializationConsantData;
		VkSpecializationInfo specializationInfo{};

		SpecializationConstantState constantState;

		// TODO: Delete this
		DescriptorSetInfo descriptorSetInfos[s_MaxDescrptorSetNum];

		// key: set << 6 | binding
		std::unordered_map<uint8_t, ShaderResource> shaderResourceMap;
		// key: set
		std::unordered_map<uint8_t, std::vector<ShaderResource>> setResources;

		std::unordered_map<std::string, ShaderResource> pushConstants;
		std::unordered_map<uint8_t, ShaderResource> specializationConstants;

		void UpdateDescriptorSetInfo(DescriptorSetInfo &setInfo, uint32_t set = 0) const;

		virtual void Init(VkDevice device);
		virtual void Destroy(VkDevice device);

		virtual std::vector<const Shader*> GetPipelineShaders() const = 0;
		virtual bool ShadersValid() const = 0;

		void SetSpecializationConstant(uint32_t constantId, uint32_t value);

		void GatherDescriptors();
		void SpirvCrossGatherDescriptors();
		std::vector<VkPipelineShaderStageCreateInfo> GetShaderStagesCreateInfo() const;
		std::vector<VkDescriptorSetLayoutBinding> GetDescriptorBindings(uint32_t set = 0) const;
		std::vector<VkDescriptorUpdateTemplateEntry> GetDescriptorUpdateTemplateEntries(uint32_t set = 0) const;
		VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, bool pushDescriptorsSupported = true) const;
		std::vector<VkDescriptorSetLayout> CreateDescriptorSetLayouts(VkDevice device, bool pushDescriptorsSupported = true) const;
		VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, uint32_t setIndex = 0, bool pushDescriptorsSupported = true) const;
		std::vector<VkPushConstantRange> CreatePushConstantRanges() const;
		VkSpecializationInfo CreateSpecializationInfo();
		VkPipelineLayout CreatePipelineLayout(VkDevice device, bool pushDescriptorSupported = true) const;
	};

	class GraphicsPipeline : public Pipeline
	{
	public:
		// Shaders
		const Shader *vertShader{ nullptr };
		const Shader *taskShader{ nullptr };
		const Shader *meshShader{ nullptr };
		const Shader *fragShader{ nullptr };

		// States
		GraphicsPipelineState pipelineState;

		std::vector<VkFormat> colorAttachmentFormats;
		VkFormat depthAttachmentFormat{ VK_FORMAT_UNDEFINED };

		virtual void Init(VkDevice device) override;
		virtual void Destroy(VkDevice device) override;

		virtual std::vector<const Shader*> GetPipelineShaders() const override;

		virtual bool ShadersValid() const override
		{
			return (fragShader != nullptr && fragShader->IsValid()) && 
				(vertShader != nullptr && vertShader->IsValid()|| meshShader != nullptr && meshShader->IsValid());
		}

		void SetAttachments(const VkFormat* pColorAttachmentFormats, uint32_t colorAttachmentCount, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED);
	};

	class ComputePipeline : public Pipeline
	{
	public:
		const Shader *compShader{ nullptr };

		virtual void Init(VkDevice device) override;
		virtual void Destroy(VkDevice device) override;

		virtual std::vector<const Shader*> GetPipelineShaders() const override
		{
			std::vector<const Shader*> shaders;
			if (compShader) 
				shaders.push_back(compShader);
			return shaders;
		}
		virtual bool ShadersValid() const override
		{
			return compShader != nullptr && compShader->IsValid();
		}
	};
}
