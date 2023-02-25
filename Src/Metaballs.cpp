#include "Metaballs.h"
#include "MarchingCubesLookup.h"
#include "Device.h"
#include "CommandManager.h"
#include <glm/gtc/random.hpp>


namespace Niagara
{
	Metaballs g_Metaballs;

	void Metaballs::Init(const Device& device, const std::vector<VkFormat>& colorAttachmentFormats, VkFormat depthFormat)
	{
		// Shaders
		{
			metaballTaskShader.Load(device, "./CompiledShaders/Metaball.task.spv");
			metaballMeshShader.Load(device, "./CompiledShaders/Metaball.mesh.spv");
			metaballFragShader.Load(device, "./CompiledShaders/SimpleMesh.frag.spv");
			// metaballFragShader.Load(device, "./CompiledShaders/Metaball.frag.spv");
		}

		// Pipelines
		{
			metaballPipeline.taskShader = &metaballTaskShader;
			metaballPipeline.meshShader = &metaballMeshShader;
			metaballPipeline.fragShader = &metaballFragShader;

			if (depthFormat != VK_FORMAT_UNDEFINED)
			{
				auto& depthStencilState = metaballPipeline.pipelineState.depthStencilState;
				depthStencilState.depthTestEnable = VK_TRUE;
				depthStencilState.depthWriteEnable = VK_TRUE;
				depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS; // VK_COMPARE_OP_GREATER_OR_EQUAL
			}

			// Rasterization state
#if 0
			{
				auto& rasterizationState = metaballPipeline.pipelineState.rasterizationState;
				rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
			}
#endif

			metaballPipeline.SetAttachments(colorAttachmentFormats.data(), static_cast<uint32_t>(colorAttachmentFormats.size()), depthFormat);

			metaballPipeline.Init(device);
		}

		// Resources
		{
			VkDeviceSize size = sizeof(g_MarchingCubesLookup);
			marchingCubesLookupBuffer.Init(device, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO, 0, g_MarchingCubesLookup, size);

			// Metaball data
			{
				size = sizeof(MetaBallData) * s_MaxBallCount;
				metaballBuffer.Init(device, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, 
					VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
			}
		}
	}

	void Metaballs::Destroy(const Device& device)
	{
		marchingCubesLookupBuffer.Destroy(device);
		metaballBuffer.Destroy(device);

		metaballPipeline.Destroy(device);

		metaballTaskShader.Cleanup(device);
		metaballMeshShader.Cleanup(device);
		metaballFragShader.Cleanup(device);
	}
}
