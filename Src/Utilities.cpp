#include "Utilities.h"
#include <spirv_cross/spirv.h>
#include <spirv-headers/spirv.h>
#include <fstream>

namespace Niagara
{
	std::vector<char> ReadFile(const std::string& fileName)
	{
		// ate - start reading at the end of the file
		// The advance of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer
		std::ifstream file(fileName, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		// After that, we can seek back to the beginning of the file and read all of the bytes at once
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	void ParseShader(const uint32_t* code, uint32_t codeSize)
	{
		assert(code[0] == SpvMagicNumber);

		uint32_t idBound = code[3];
	}

	VkShaderModule LoadShader(VkDevice device, const std::string& fileName)
	{
		std::vector<char> byteCode = ReadFile(fileName);

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = byteCode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

		return shaderModule;
	}
}