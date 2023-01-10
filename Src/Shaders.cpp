#include "Shaders.h"
#include "Utilities.h"
#include <spirv-headers/spirv.h>
#include <spirv_cross/spirv.h>

// Ref: https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html


namespace Niagara
{
	VkShaderModule Shader::LoadShader(VkDevice device, const std::string& fileName)
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

	struct Id
	{
		uint32_t opCode;
		uint32_t typeId;
		uint32_t storageClass;
		uint32_t binding;
		uint32_t set;
		uint32_t constant;
	};

	static VkShaderStageFlagBits GetShaderStage(SpvExecutionModel executionModel)
	{
		switch (executionModel)
		{
		case SpvExecutionModelVertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case SpvExecutionModelFragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case SpvExecutionModelGLCompute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case SpvExecutionModelTaskEXT:
			return VK_SHADER_STAGE_TASK_BIT_EXT;
		case SpvExecutionModelTaskNV:
			return VK_SHADER_STAGE_TASK_BIT_NV;
		case SpvExecutionModelMeshEXT:
			return VK_SHADER_STAGE_MESH_BIT_EXT;
		case SpvExecutionModelMeshNV:
			return VK_SHADER_STAGE_MESH_BIT_NV;

		default:
			assert(!"Unsupported exetution model");
			return VkShaderStageFlagBits(0);
		}
	}

	static VkDescriptorType GetDescriptorType(SpvOp op)
	{
		switch (op)
		{
		case SpvOpTypeStruct:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case SpvOpTypeImage:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case SpvOpTypeSampler:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		case SpvOpTypeSampledImage:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		default:
			assert(!"Unknown resource type");
			return VkDescriptorType(0);
		}
	}

	void Shader::ParseShader(Shader& shader, const uint32_t* code, size_t codeSize)
	{
		assert(code[0] == SpvMagicNumber);

		uint32_t idBound = code[3];

		std::vector<Id> idArray(idBound);

		const uint32_t *instruction = code + 5;
		const uint32_t *codeEnd = code + codeSize;
		while (instruction != codeEnd)
		{
			auto opCode = static_cast<uint16_t>(instruction[0] & SpvOpCodeMask);
			auto wordCount = static_cast<uint16_t>(instruction[0] >> SpvWordCountShift);

			switch (opCode)
			{
			case SpvOpEntryPoint:
			{
				assert(wordCount >= 2);
				shader.stage = GetShaderStage(SpvExecutionModel(instruction[1]));
				break;
			}
			case SpvOpExecutionMode:
			{
				assert(wordCount >= 3);
				uint32_t mode = instruction[2];

				switch (mode)
				{
				case SpvExecutionModeLocalSize:
					assert(wordCount == 6);
					auto localSizeX = instruction[3];
					auto localSizeY = instruction[4];
					auto localSizeZ = instruction[5];
					break;
				}
			} break;
			case SpvOpExecutionModeId:
			{
				assert(wordCount >= 3);
				uint32_t mode = instruction[2];
				switch (mode)
				{
					case SpvExecutionModeLocalSize:
						assert(wordCount == 6);
						auto localSizeX = instruction[3];
						auto localSizeY = instruction[4];
						auto localSizeZ = instruction[5];
						break;
				}
				break;
			}
			case SpvOpDecorate:
			{
				assert(wordCount >= 3);
				
				uint32_t id = instruction[1];
				assert(id < idBound);

				switch (instruction[2])
				{
					// OpDecorate %20 DescriptorSet 0
				case SpvDecorationDescriptorSet:
					assert(wordCount == 4);
					idArray[id].set = instruction[3];
					break;
				case SpvDecorationBinding:
					assert(wordCount == 4);
					idArray[id].binding = instruction[3];
					break;
				}
				
				break;
			}
			case SpvOpTypeStruct:
			case SpvOpTypeImage:
			case SpvOpTypeSampler:
			case SpvOpTypeSampledImage:
			{
				assert(wordCount >= 2);

				uint32_t id = instruction[1];
				assert(id < idBound);

				assert(idArray[id].opCode == 0);
				idArray[id].opCode = opCode;

				break;
			}
			case SpvOpTypePointer:
			{
				assert(wordCount == 4);

				uint32_t id = instruction[1];
				assert(id < idBound);

				assert(idArray[id].opCode == 0);
				idArray[id].opCode = opCode;
				idArray[id].typeId = instruction[3];
				idArray[id].storageClass = instruction[2];

				break;
			}
			case SpvOpConstant:
			{
				assert(wordCount >= 4);

				uint32_t id = instruction[2];
				assert(id < idBound);

				assert(idArray[id].opCode == 0);
				idArray[id].opCode = opCode;
				idArray[id].typeId = instruction[1];
				idArray[id].constant = instruction[3];

				break;
			}
			case SpvOpVariable:
			{
				assert(wordCount >= 4);

				uint32_t id = instruction[2];
				assert(id < idBound);

				assert(idArray[id].opCode == 0);
				idArray[id].opCode = opCode;
				idArray[id].typeId = instruction[1];
				idArray[id].storageClass = instruction[3];

				break;	
			}

			}

			assert(instruction + wordCount <= code + codeSize);
			instruction += wordCount;
		}

		for (const auto &id : idArray)
		{
			if (id.opCode == SpvOpVariable && 
			   (id.storageClass == SpvStorageClassUniform || id.storageClass == SpvStorageClassUniformConstant || id.storageClass == SpvStorageClassStorageBuffer))
			{
				assert(id.set == 0);
				assert(id.binding < 32);
				assert(idArray[id.typeId].opCode == SpvOpTypePointer);

				uint32_t typeKind = idArray[idArray[id.typeId].typeId].opCode;
				VkDescriptorType resourceType = GetDescriptorType(SpvOp(typeKind));

				shader.resourceTypes[id.binding] = resourceType;
				shader.resourceMask |= 1 << id.binding;

			}

			if (id.opCode == SpvOpVariable && id.storageClass == SpvStorageClassPushConstant)
				shader.usePushConstants = true;
		}
	}

	bool Shader::LoadShader(VkDevice device, Shader &shader, const std::string& fileName)
	{
		std::vector<char> byteCode = ReadFile(fileName);

		uint32_t *pCode = reinterpret_cast<uint32_t*>(byteCode.data());
		size_t codeSize = byteCode.size();

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = codeSize;
		createInfo.pCode = pCode;

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
		assert(shaderModule);

		ParseShader(shader, pCode, codeSize/4);

		shader.module = shaderModule;

		return true;
	}
}