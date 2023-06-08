#pragma once

#include "pch.h"
#include "VkCommon.h"
#include "Utilities.h"
#include <vk_mem_alloc.h>


namespace Niagara
{
	class Device;

	class Buffer
	{
	public:
		VkBuffer buffer{ VK_NULL_HANDLE };
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VmaAllocation allocation{ VK_NULL_HANDLE };
		
		VkDeviceSize size{ 0 };
		VkDeviceSize memOffset{ 0 };
		VkBufferUsageFlags bufferUsage{ 0 };

		std::string name;
		uint8_t* mappedData{ nullptr };
		// Whether the buffer has been mapped with vmaMapMemory
		bool mapped{ false };
		// Whether the buffer is persistently mapped or not
		bool persistent{ false };

		static void Copy(Buffer& dstBuffer, const Buffer& srcBuffer);

		Buffer(const std::string &inName = "") : name{inName} { }
		// NON_COPYABLE(Buffer);

		operator VkBuffer() const { return buffer; }

		void Init(const Device& device, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO, VmaAllocationCreateFlags allocFlags = 0, const void* pInitData = nullptr, size_t initSize = 0);
		void Destroy(const Device& device);

		void Update(const void* data, size_t size, size_t offset = 0);

		// Maps Vulkan memory if it hasn't already mapped to an host visible address
		uint8_t* Map(const Device &device);
		// Unmaps Vulkan memory from the host visible address
		void Unmap(const Device& device);
		// Flushes memory if it is HOST_VISIBLE and not HOST_COHERENT
		void Flush(const Device& device) const;

		DescriptorInfo GetDescriptorInfo() const
		{
			return DescriptorInfo(buffer, 0, size);
		}
	};
}
