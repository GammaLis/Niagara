#version 450

#extension GL_GOOGLE_include_directive  : require

#include "Common.h"

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

void main()
{
	vec4 color =  vec4(uv, 0.0, 1.0);

#if 0
	bool insideBound = false;
	for (uint i = 0; i < 3; ++i)
	{
		vec4 aabb = vec4(0.0);
		insideBound = GetAxisAlignedBoundingBox(_View.bounds[i], -_View.zNearFar.x, _View.projMatrix, aabb);
		insideBound = insideBound && InsideBound(uv, aabb);
		if (insideBound) break;
	}

	
	if (insideBound)
		color = vec4(1.0, 0.0, 0.0, 1.0);
#endif

	outColor = color;
}
