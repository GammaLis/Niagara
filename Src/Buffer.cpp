#include "Buffer.h"
#include "Device.h"
#include "CommandManager.h"
#include "Renderer.h"


namespace Niagara
{
	void Init(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags, const void* pInitData)
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
		uint8_t* mappedData;
		bool mapped{ false };

		VkBufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;
		createInfo.usage = usage;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Optional

		VK_CHECK(vkCreateBuffer(device, &createInfo, nullptr, &buffer));

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;

		VkBool32 memTypeFound = VK_FALSE;
		allocInfo.memoryTypeIndex = device.GetMemoryType(memRequirements.memoryTypeBits, memoryFlags, &memTypeFound);

		VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));

		VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

		if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			vkMapMemory(device, memory, 0, size, 0, reinterpret_cast<void**>(&mappedData));
			mapped = true;
		}
	}

	void Buffer::Copy(Buffer& dstBuffer, const Buffer& srcBuffer)
	{
		VkCommandBuffer cmd = BeginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = srcBuffer.size;
		vkCmdCopyBuffer(cmd, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);

		EndSingleTimeCommands(cmd);
	}

	void Buffer::Init(const Device& device, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags, const void* pInitData, size_t initSize)
	{
		Destroy(device);

		this->bufferUsage = bufferUsage;
		this->persistent = (allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;

		VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		createInfo.size = size;
		createInfo.usage = bufferUsage;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // optional
		
		VmaAllocationCreateInfo memInfo{};
		memInfo.flags = allocFlags;
		memInfo.usage = memoryUsage;

		VmaAllocationInfo allocInfo{};
		VK_CHECK(vmaCreateBuffer(device.memoryAllocator, &createInfo, &memInfo, &buffer, &allocation, &allocInfo));

		this->memory = allocInfo.deviceMemory;
		this->memOffset = allocInfo.offset;
		this->size = allocInfo.size;

		if (persistent)
			mappedData = reinterpret_cast<uint8_t*>(allocInfo.pMappedData);

		if (pInitData != nullptr)
			Update(pInitData, initSize);

		if (name != "")
			g_AccessMgr.AddResourceAccess(name);
	}

	void Buffer::Destroy(const Device& device)
	{
		if (buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE)
		{
			Unmap(device);
			vmaDestroyBuffer(device.memoryAllocator, buffer, allocation);

			buffer = VK_NULL_HANDLE;
			allocation = VK_NULL_HANDLE;
		}
	}

	void Buffer::Update(const void* data, size_t size, size_t offset)
	{
		const Device& device = *g_Device;

		if (persistent)
		{
			uint8_t* mappedData = Map(device);
			memcpy_s(mappedData + offset, size, data, size);
			Flush(device);
		}
		else
		{
			Buffer stagingBuffer;
			stagingBuffer.Init(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, data, size);

			Copy(*this, stagingBuffer);

			stagingBuffer.Destroy(device);
		}
	}

	uint8_t* Buffer::Map(const Device& device)
	{
		if (!mapped && !mappedData)
		{
			VK_CHECK(vmaMapMemory(device.memoryAllocator, allocation, reinterpret_cast<void**>(&mappedData)));
			mapped = true;
		}

		return mappedData;
	}

	void Buffer::Unmap(const Device& device)
	{
		if (mapped)
		{
			vmaUnmapMemory(device.memoryAllocator, allocation);
			mappedData = nullptr;
			mapped = false;
		}
	}

	void Buffer::Flush(const Device &device) const
	{
		vmaFlushAllocation(device.memoryAllocator, allocation, 0, size);
	}
}
