#include "VkQuery.h"
#include "Device.h"

namespace Niagara
{
	CommonQueryPools g_CommonQueryPools;


	void QueryPool::Init(const Device& device, VkQueryType type, uint32_t count, VkQueryPipelineStatisticFlags pipelineStatisticFlags)
	{
		Destroy(device);

		this->type = type;
		this->count = count;
		this->pipelineStaticsFlags = pipelineStaticsFlags;

		VkQueryPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		createInfo.queryType = type;
		createInfo.queryCount = count;
		createInfo.pipelineStatistics = pipelineStaticsFlags;

		queryPool = VK_NULL_HANDLE;
		VK_CHECK(vkCreateQueryPool(device, &createInfo, nullptr, &queryPool));
	}

	void QueryPool::Destroy(const Device& device)
	{
		if (queryPool != VK_NULL_HANDLE)
			vkDestroyQueryPool(device, queryPool, nullptr);
	}

	void QueryPool::BeginQuery(VkCommandBuffer cmd, uint32_t query)
	{
		activeQueryCount++;

		vkCmdBeginQuery(cmd, queryPool, query, 0);
	}

	void QueryPool::EndQuery(VkCommandBuffer cmd, uint32_t query)
	{
		assert(activeQueryCount > 0);

		vkCmdEndQuery(cmd, queryPool, query);
	}

	void QueryPool::WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage, uint32_t query)
	{
		vkCmdWriteTimestamp(cmd, pipelineStage, queryPool, query);
	}

	void QueryPool::Reset(VkCommandBuffer cmd, uint32_t firstQuery, uint32_t queryCount)
	{
		queryCount = std::min(queryCount, count);
		activeQueryCount = 0;
		vkCmdResetQueryPool(cmd, queryPool, firstQuery, queryCount);
	}

	void QueryPool::Reset(const Device &device, uint32_t firstQuery, uint32_t queryCount)
	{
		activeQueryCount = 0;
		vkResetQueryPool(device, queryPool, firstQuery, queryCount);
	}

	VkResult QueryPool::GetResults(const Device &device, uint32_t firstQeury, uint32_t queryCount, size_t resultBytes, void* results, VkDeviceSize stride, VkQueryResultFlags flags)
	{
		if (queryPool == VK_NULL_HANDLE) 
			return VK_NOT_READY;

		return vkGetQueryPoolResults(device, queryPool, firstQeury, queryCount, resultBytes, results, stride, flags);
	}


	void CommonQueryPools::Init(const Device& device)
	{
		queryPools[0].Init(device, VK_QUERY_TYPE_TIMESTAMP, QueryPool::s_MaxQueryCount);
		// VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT specifies that queries managed by the pool will count 
		// the number of primitives processed by the Primitive Clipping stage of the pipeline. The counter¡¯s value is 
		// incremented each time a primitive reaches the primitive clipping stage.
		queryPools[1].Init(device, VK_QUERY_TYPE_PIPELINE_STATISTICS, 4,
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT); // VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT
	}

	void CommonQueryPools::Destroy(const Device& device)
	{
		queryPools[0].Destroy(device);
		queryPools[1].Destroy(device);
	}
}
