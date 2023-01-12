#pragma once

#include "pch.h"


namespace Niagara
{
	struct DescriptorInfo
	{
		union
		{
			VkDescriptorBufferInfo bufferInfo;
			VkDescriptorImageInfo imageInfo;
		};

		DescriptorInfo() {  }

		DescriptorInfo(VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE)
		{
			bufferInfo.buffer = buffer;
			bufferInfo.offset = offset;
			bufferInfo.range = range;
		}
		DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
		{
			imageInfo.sampler = sampler;
			imageInfo.imageView = imageView;
			imageInfo.imageLayout = imageLayout;
		}
	};


	VkShaderModule LoadShader(VkDevice device, const std::string& fileName);
	
	class Shader
	{
	public:
		using ShaderList = std::initializer_list<Shader*>;
		using ConstantList = std::initializer_list<uint32_t>;

		VkShaderModule module = VK_NULL_HANDLE;
		VkShaderStageFlagBits stage;

		VkDescriptorType resourceTypes[32];
		uint32_t resourceMask;

		bool usePushConstants = false;

		static void ParseShader(Shader& shader, const uint32_t* code, size_t codeSize);
		static bool LoadShader(VkDevice device, Shader& shader, const std::string& fileName);

		static uint32_t GatherResources(const std::vector<const Shader*>& shaders, VkDescriptorType(&resourceTypes)[32]);
		static std::vector<VkDescriptorSetLayoutBinding> GetSetBindings(const std::vector<const Shader*>& shaders, const VkDescriptorType resourceTypes[] = nullptr, uint32_t resourceMask = 0);
		static std::vector<VkDescriptorUpdateTemplateEntry> GetUpdateTemplateEntries(const std::vector<const Shader*>& shaders, VkDescriptorType resourceTypes[] = nullptr, uint32_t resourceMask = 0);
		static VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<const Shader*>& shaders, bool pushDescriptorsSupported = true);
		static VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkDescriptorSetLayout setLayout, const std::vector<const Shader*> &shaders, bool pushDescriptorsSupported = true);

		bool Load(VkDevice device, const std::string &fileName)
		{
			Cleanup(device);

			return LoadShader(device, *this, fileName);
		}

		void Cleanup(VkDevice device)
		{
			if (module)
				vkDestroyShaderModule(device, module, nullptr);

			stage = VkShaderStageFlagBits(0);
			resourceMask = 0;
			usePushConstants = false;
		}
	};
}
