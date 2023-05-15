// Ref: Vulkan-samples

#pragma once

#include "pch.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_XYZW_ONLY
#include <glm/glm.hpp>


namespace Niagara
{

#define NON_COPYABLE(T) \
	T(const T&) = delete; T& operator=(const T&) = delete

#define NON_MOVABLE(T) \
	T(T&&) = delete; T& operator=(T&&) = delete

	/// Files

	std::vector<char> ReadFile(const std::string& fileName);


	/// Math

	constexpr float EPS = 1e-5f;

	// Ref: UE - PlatformMath

	inline bool IsNaN(float a) { return _isnan(a) != 0; }
	inline bool IsNaN(double a) { return _isnan(a) != 0; }
	inline bool IsFinite(float a) { return _finite(a) != 0; }
	inline bool IsFinite(double a) { return _finite(a) != 0; }

	inline uint32_t FloorLog2(uint32_t val)
	{
		// Use BSR to return the log2 of the integer
		unsigned long log2;
		if (_BitScanReverse(&log2, val) != 0)
			return log2;

		return 0;
	}

	inline uint32_t CountLeadingZeros(uint32_t val)
	{
		unsigned long log2;
		long mask = -long(_BitScanReverse(&log2, val) != 0);
		return ((31 - log2) & mask) | (32 & ~mask);
	}

	inline uint32_t CountTrailingZeros(uint32_t val)
	{
		if (val == 0)
			return 32;

		unsigned long bitIndex;	// 0-based, where the LSB is 0 and MSB is 31
		_BitScanForward(&bitIndex, val);	// Scans from LSB to MSB
		return bitIndex;
	}

	inline uint32_t CeilLogTwo(uint32_t val)
	{
		int32_t bitmask = ((int32_t)(CountLeadingZeros(val) << 26)) >> 31;
		return (32 - CountLeadingZeros(val - 1)) & (~bitmask);
	}

	inline uint32_t RoundUpToPowerOfTwo(uint32_t val)
	{
		return 1 << CeilLogTwo(val);
	}

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

	inline uint32_t DivideAndRoundUp(uint32_t dividend, uint32_t divisor)
	{
		return (dividend + divisor - 1) / divisor;
	}

	inline uint32_t DivideAndRoundDown(uint32_t dividend, uint32_t divisor)
	{
		return dividend / divisor;
	}

	inline uint32_t GetMipLevels(uint32_t width, uint32_t height)
	{
		uint32_t highBit;
		_BitScanReverse((unsigned long*)&highBit, width | height);
		return highBit + 1;
	}

	inline glm::vec3 SafeNormalize(const glm::vec3 &v)
	{
		float n = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
		return (n < EPS) ? glm::vec3(0.0f) : v / n;
	}

	inline glm::vec4 GetSizeAndInvSize(uint32_t width, uint32_t height)
	{
		return glm::vec4(width, height, 1.0f/width, 1.0f/height);
	}

	// https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
	inline glm::mat4 MakeInfReversedZProjRH(float fovy, float aspect, float zNear)
	{
		float f = 1.0f / tan(fovy / 2.0f);
		return glm::mat4(
			f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f,	 f,  0.0f,  0.0f,
			0.0f, 0.0f,  0.0f, -1.0f,
			0.0f, 0.0f, zNear,  0.0f);
	}

	void GetFrustumPlanes(glm::vec4 frustumPlanes[], const glm::mat4& matrix, bool reversedZ = true, bool needZPlanes = true);

	/// Miscs

	double GetSystemTime();
}
