#pragma once

#include "pch.h"


namespace Niagara
{
	class Device;

	class QueryPool 
	{
	public:
		static constexpr uint32_t s_MaxQueryCount = 128;

		VkQueryPool queryPool{ VK_NULL_HANDLE };
		VkQueryType type;
		uint32_t count;
		VkQueryPipelineStatisticFlags pipelineStaticsFlags;

	private:
		uint32_t activeQueryCount{ 0 };

	public:
		QueryPool() = default;

		void Init(const Device &device, VkQueryType type, uint32_t count, VkQueryPipelineStatisticFlags pipelineStatisticFlags = VkQueryControlFlagBits(0));
		void Destroy(const Device &device);

		operator VkQueryPool() const { return queryPool; }

		void BeginQuery(VkCommandBuffer cmd, uint32_t query = s_MaxQueryCount);
		void EndQuery(VkCommandBuffer cmd, uint32_t query = s_MaxQueryCount);
		void WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage, uint32_t query);
		void Reset(VkCommandBuffer cmd, uint32_t firstQuery = 0, uint32_t queryCount = s_MaxQueryCount);
		void Reset(const Device &device, uint32_t firstQuery = 0, uint32_t queryCount = s_MaxQueryCount);
		VkResult GetResults(const Device &device, uint32_t firstQeury, uint32_t queryCount, size_t resultBytes, void* results,
			VkDeviceSize stride, VkQueryResultFlags flags);
	};

	struct CommonQueryPools
	{
		QueryPool queryPools[2];

		void Init(const Device& device);
		void Destroy(const Device& device);
	};
	extern CommonQueryPools g_CommonQueryPools;
}
