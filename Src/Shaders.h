#pragma once

#include "pch.h"

namespace Niagara
{
	class Shader
	{
	public:
		VkShaderModule module;
		VkShaderStageFlagBits stage;

		VkDescriptorType resourceTypes[32];
		uint32_t resourceMask;

		bool usePushConstants = false;

		static VkShaderModule LoadShader(VkDevice device, const std::string& fileName);
		static void ParseShader(Shader& shader, const uint32_t* code, size_t codeSize);
		static bool LoadShader(VkDevice device, Shader &shader, const std::string& fileName);
	};
}
