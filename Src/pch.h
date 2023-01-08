#pragma once

#include <string>
#include <vector>
#include <array>
#include <set>
#include <cassert>
#include <optional>
#include <cstdint>
#include <limits>		// std::numeric_limits
#include <algorithm>	// std::clamp

#define NOMINMAX

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
