// Ref: Vulkan-Samples: spirv-reflection

#pragma once

#include "pch.h"
#include "spirv_cross.hpp"


namespace Niagara
{
	class Shader;

	void ReflectShaderInfos(Shader &shader, uint32_t* shaderCode, size_t wordCount);
}
