#pragma once

#include "pch.h"

#define USE_SPIRV_CROSS 1


namespace Niagara
{
	// Types of shader resources
	enum class ShaderResourceType
	{
		Input,
		InputAttachment,
		Output,
		Image,
		ImageSampler,
		ImageStorage,
		Sampler,
		BufferUniform,
		BufferStorage,
		PushConstant,
		SpecializationConstant,
		All
	};

	// This determines the type and method of how descriptor set should be created and bound
	enum class ShaderResourceMode
	{
		Static,
		Dynamic,
		UpdateAfterBind
	};

	// A bitmask of qualifiers applied to a resource
	struct ShaderResourceQualifiers
	{
		enum : uint32_t
		{
			None = 0,
			NonReadable = 1,
			NonWritable = 2,
		};
	};

	VkDescriptorType GetDescriptorType(ShaderResourceType resourceType, bool bDynamic = false);

	// Store shader resource data
	struct ShaderResource
	{
		VkShaderStageFlags stages;
		ShaderResourceType type;
		ShaderResourceMode mode;

		uint32_t set;
		uint32_t binding;
		uint32_t location;
		uint32_t inputAttachmentIndex;
		uint32_t vecSize;
		uint32_t columns;
		uint32_t arraySize;
		uint32_t offset;
		uint32_t size;
		uint32_t constantId;
		uint32_t qualifiers;
		std::string name;
	};

	struct DescriptorSetInfo
	{
		uint32_t start = 0;
		uint32_t count = 0;
		uint32_t mask = 0;
		VkDescriptorType types[32] = {};
	};

	VkShaderModule LoadShader(VkDevice device, const std::string& fileName);

	class Device;
	
	class Shader
	{
	public:
		using ShaderList = std::initializer_list<Shader*>;
		using ConstantList = std::initializer_list<uint32_t>;

		VkShaderModule module = VK_NULL_HANDLE;
		VkShaderStageFlagBits stage{};

		VkDescriptorType resourceTypes[32];
		uint32_t resourceMask;
		bool usePushConstants = false;

		std::string entryPoint{"main"};
		std::vector<ShaderResource> resources;

		static void ParseShader(Shader& shader, const uint32_t* code, size_t codeSize);
		static bool LoadShader(VkDevice device, Shader& shader, const std::string& fileName);

		static uint32_t GatherResources(const std::vector<const Shader*>& shaders, VkDescriptorType(&resourceTypes)[32]);
		static std::vector<VkDescriptorSetLayoutBinding> GetSetBindings(const std::vector<const Shader*>& shaders, const VkDescriptorType resourceTypes[] = nullptr, uint32_t resourceMask = 0);
		static std::vector<VkDescriptorUpdateTemplateEntry> GetUpdateTemplateEntries(const std::vector<const Shader*>& shaders, VkDescriptorType resourceTypes[] = nullptr, uint32_t resourceMask = 0);
		static VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<const Shader*>& shaders, bool pushDescriptorsSupported = true);
		static VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkDescriptorSetLayout setLayout, const std::vector<const Shader*> &shaders, bool pushDescriptorsSupported = true);

		Shader() = default;

		operator VkShaderModule() const { return module; }

		bool IsValid() const { return module != VK_NULL_HANDLE; }

		bool Load(const Device &device, const std::string &fileName);

		void Cleanup(const Device& device);
	};
}
