#include "SpirvReflection.h"
#include "Shaders.h"
#include "spirv_glsl.hpp"
#include <iostream>


using namespace spirv_cross;

namespace Niagara
{
	static void ReadResourceVecSize(const Compiler& compiler, ShaderResource &shaderResource, const Resource &resource)
	{
		const auto &type = compiler.get_type_from_variable(resource.id);

		shaderResource.vecSize = type.vecsize;
		shaderResource.columns = type.columns;
	}

	static void ReadResourceArraySize(const Compiler& compiler, ShaderResource &shaderResource, const Resource &resource)
	{
		const auto& type = compiler.get_type_from_variable(resource.id);

		shaderResource.arraySize = !type.array.empty() ? type.array[0] : 1;
	}

	static void ReadResourceSize(const Compiler& compiler, ShaderResource &shaderResource, const Resource &resource)
	{
		const auto &type = compiler.get_type_from_variable(resource.id);

		size_t arraySize = 0;
		shaderResource.size = static_cast<uint32_t>(compiler.get_declared_struct_size_runtime_array(type, arraySize));
	}

	static void ReadResourceSize(const Compiler& compiler, ShaderResource &shaderResource, const SPIRConstant &constant)
	{
		const auto &type = compiler.get_type(constant.constant_type);

		switch (type.basetype)
		{
		case SPIRType::BaseType::Boolean:
		case SPIRType::BaseType::Char:
		case SPIRType::BaseType::Int:
		case SPIRType::BaseType::UInt:
		case SPIRType::BaseType::Float:
			shaderResource.size = 4;
			break;

		case SPIRType::BaseType::Int64:
		case SPIRType::BaseType::UInt64:
		case SPIRType::BaseType::Double:
			shaderResource.size = 8;
			break;

		default:
			shaderResource.size = 0;
			break;
		}
	}


	static void ParseShaderResources(const Compiler& compiler, std::vector<ShaderResource> &shaderResources, VkShaderStageFlagBits stage)
	{
		const auto &resources = compiler.get_shader_resources() ;

		// Inputs
		const auto &inputs = resources.stage_inputs;
		for (const auto& resource : inputs)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::Input;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceVecSize(compiler, shaderResource, resource);
			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.location = compiler.get_decoration(resource.id, spv::DecorationLocation);

			shaderResources.push_back(shaderResource);
		}

		// Input attachments
		const auto &inputAttachments = resources.subpass_inputs;
		for (const auto& resource : inputAttachments)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::InputAttachment;
			shaderResource.stages = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderResource.name = resource.name;

			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.inputAttachmentIndex = compiler.get_decoration(resource.id, spv::DecorationInputAttachmentIndex);
			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			shaderResources.push_back(shaderResource);
		}

		// Outputs
		const auto &outputs = resources.stage_outputs;
		for (const auto& resource : outputs)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::Output;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceArraySize(compiler, shaderResource, resource);
			ReadResourceVecSize(compiler, shaderResource, resource);
			
			shaderResource.location = compiler.get_decoration(resource.id, spv::DecorationLocation);

			shaderResources.push_back(shaderResource);
		}

		// Images
		const auto &images = resources.separate_images;
		for (const auto& resource : images)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::Image;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			shaderResources.push_back(shaderResource);
		}

		// Image samplers
		const auto &imageSamplers = resources.sampled_images;
		for (const auto& resource : imageSamplers)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::ImageSampler;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			shaderResources.push_back(shaderResource);
		}

		// Image storages
		const auto &imageStorages = resources.storage_images;
		for (const auto& resource : imageStorages)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::ImageStorage;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			shaderResource.qualifiers |= ShaderResourceQualifiers::NonReadable | ShaderResourceQualifiers::NonWritable;

			shaderResources.push_back(shaderResource);
		}

		// Samplers
		const auto &samplers = resources.separate_samplers;
		for (const auto &resource : samplers)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::Sampler;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			shaderResources.push_back(shaderResource);
		}

		// Buffer uniforms
		const auto &bufferUniforms = resources.uniform_buffers;
		for (const auto &resource : bufferUniforms)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::BufferUniform;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceSize(compiler, shaderResource, resource);
			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			shaderResources.push_back(shaderResource);
		}

		// Buffer storages
		const auto &bufferStorages = resources.storage_buffers;
		for (const auto &resource : bufferStorages)
		{
			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::BufferStorage;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;

			ReadResourceSize(compiler, shaderResource, resource);
			ReadResourceArraySize(compiler, shaderResource, resource);

			shaderResource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			shaderResource.qualifiers |= ShaderResourceQualifiers::NonReadable | ShaderResourceQualifiers::NonWritable;

			shaderResources.push_back(shaderResource);
		}

	}

	static void ParsePushConstants(const Compiler& compiler, std::vector<ShaderResource> &shaderResources, VkShaderStageFlagBits stage)
	{
		auto resources = compiler.get_shader_resources();

		for (auto &resource : resources.push_constant_buffers)
		{
			const auto &type = compiler.get_type_from_variable(resource.id);

			uint32_t offset = std::numeric_limits<uint32_t>::max();
			uint32_t numMemType = static_cast<uint32_t>(type.member_types.size());
			for (uint32_t i = 0; i < numMemType; ++i)
			{
				auto memOffset = compiler.get_member_decoration(type.self, i, spv::DecorationOffset);

				offset = std::min(offset, memOffset);
			}

			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::PushConstant;
			shaderResource.stages = stage;
			shaderResource.name = resource.name;
			shaderResource.offset = offset;

			ReadResourceSize(compiler, shaderResource, resource);

			shaderResource.size -= shaderResource.offset;

			shaderResources.push_back(shaderResource);
		}
	}

	static void ParseSpecializationConstants(const Compiler& compiler, std::vector<ShaderResource> &shaderResources, VkShaderStageFlagBits stage)
	{
		const auto &constants = compiler.get_specialization_constants();

		for (auto& resource : constants)
		{
			auto& value = compiler.get_constant(resource.id);

			ShaderResource shaderResource{};
			shaderResource.type = ShaderResourceType::SpecializationConstant;
			shaderResource.stages = stage;
			shaderResource.name = compiler.get_name(resource.id);
			shaderResource.offset = 0;
			shaderResource.constantId = resource.constant_id;

			ReadResourceSize(compiler, shaderResource, value);

			shaderResources.push_back(shaderResource);
		}
	}

	static VkShaderStageFlagBits GetShaderStage(spv::ExecutionModel executionModel)
	{
		switch (executionModel)
		{
		case spv::ExecutionModel::ExecutionModelVertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case spv::ExecutionModel::ExecutionModelFragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case spv::ExecutionModel::ExecutionModelGLCompute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case spv::ExecutionModel::ExecutionModelTaskEXT:
			return VK_SHADER_STAGE_TASK_BIT_EXT;
		case spv::ExecutionModel::ExecutionModelTaskNV:
			return VK_SHADER_STAGE_TASK_BIT_NV;
		case spv::ExecutionModel::ExecutionModelMeshEXT:
			return VK_SHADER_STAGE_MESH_BIT_EXT;
		case spv::ExecutionModel::ExecutionModelMeshNV:
			return VK_SHADER_STAGE_MESH_BIT_NV;

		default:
			assert(!"Unsupported exetution model");
			return VkShaderStageFlagBits(0);
		}
	}

	void ReflectShaderInfos(Shader &shader, uint32_t* shaderCode, size_t wordCount)
	{
		CompilerGLSL compiler(shaderCode, wordCount);

		// Entry
		// TODO: multi enties ?
		const auto &entries = compiler.get_entry_points_and_stages();
		assert(!entries.empty());
		
		auto &entry = entries[0];
		shader.entryPoint = entry.name;
		shader.stage = GetShaderStage(entry.execution_model);

		// Resources
		std::vector<ShaderResource>& shaderResources = shader.resources;
		shaderResources.clear();

		ParseShaderResources(compiler, shaderResources, shader.stage);
		ParsePushConstants(compiler, shaderResources, shader.stage);
		ParseSpecializationConstants(compiler, shaderResources, shader.stage);
	}
}
