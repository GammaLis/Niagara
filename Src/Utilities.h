// Ref: Vulkan-samples

#pragma once

#include "pch.h"
#include <glm/glm.hpp>


namespace Niagara
{
	/// Files

	std::vector<char> ReadFile(const std::string& fileName);


	/// Math

	constexpr float EPS = 1e-5f;

	inline float ToFloat(uint16_t v)
	{
		uint16_t sign = (v >> 15);
		uint16_t exp = (v >> 10) & 31;
		uint16_t significant = v & 1023;

		assert(exp != 31);
		if (exp == 0)
		{
			if (significant == 0)
				return 0;
			else
				return (sign > 0 ? -1.0f : 1.0f) * powf(2.0f, -14.0f) * (significant / 1024.0f);
		}
		else
		{
			return (sign > 0 ? -1.0f : 1.0f) * powf(2.0f, (float)exp - 15.0f) * ((1024 + significant) / 1024.0f);
		}
	}

	inline int DivideAndRoundUp(int dividend, int divisor)
	{
		return (dividend + divisor - 1) / divisor;
	}

	inline int DivideAndRoundDown(int dividend, int divisor)
	{
		return dividend / divisor;
	}

	inline glm::vec3 SafeNormalize(const glm::vec3 &v)
	{
		float n = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
		return (n < EPS) ? glm::vec3(0.0f) : v / n;
	}


	/// Vulkan

	// Determine if a Vulkan format is depth only.
	inline bool IsDepthOnlyFormat(VkFormat format)
	{
		return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
	}

	// Determine if a Vulkan format is depth or stencil
	inline bool IsDepthStencilFormat(VkFormat format)
	{
		return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
			IsDepthOnlyFormat(format);
	}

	// Determine if a Vulkan descriptor type is a dynamic storage buffer or dynamic uniform buffer.
	inline bool IsDynamicBufferDescriptorType(VkDescriptorType descriptorType)
	{
		return descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC || descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	}

	// Determine if a Vulkan descriptor type is a buffer (either uniform or storage buffer, dynmaic or not).
	inline bool IsBufferDescriptorType(VkDescriptorType descriptorType)
	{
		return descriptorType  == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			IsDynamicBufferDescriptorType(descriptorType);
	}

	uint32_t BitsPerPixel(VkFormat format);


	/// Miscs

	double GetSystemTime();
}
