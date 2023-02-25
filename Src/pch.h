#pragma once

#include <string>
#include <vector>
#include <array>
#include <set>
#include <memory>
#include <cassert>
#include <system_error>
#include <cstdint>
#include <limits>		// std::numeric_limits
#include <algorithm>	// std::clamp

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Vulkan

#include <volk.h>

#if 1
#include <vulkan/vulkan.h>
#else
// A bit slow
#include <vulkan/vulkan.hpp>
#endif

#define VK_CHECK(call) \
	do { \
		VkResult result = call; \
		assert(result == VK_SUCCESS); \
	} while (0)
