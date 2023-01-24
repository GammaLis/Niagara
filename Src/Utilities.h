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


	/// Miscs

	double GetSystemTime();
}
