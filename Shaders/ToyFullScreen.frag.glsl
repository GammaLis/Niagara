#version 450

#extension GL_GOOGLE_include_directive  : require

#include "Common.h"
#include "MeshCommon.h"

layout (binding = 0) readonly buffer Meshes
{
	Mesh meshes[];
};

layout (binding = 1) readonly buffer Draws
{
	MeshDraw draws[];
};

layout (binding = 3) readonly buffer DrawCommandCount
{
	uint drawCounts[];
};

layout (binding = 4) readonly buffer DrawVisibilities
{
	uint drawVisibilities[];
};


layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

vec3 DebugColors[] = 
{
	vec3(0.0, 0.0, 0.0),

	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),

	vec3(1.0, 1.0, 0.0),
	vec3(1.0, 0.0, 1.0),
	vec3(0.0, 1.0, 1.0),

	vec3(1.0, 1.0, 1.0)
};

void main()
{
	const vec2 fragUV = (gl_FragCoord.xy + 0.5) / _View.viewportRect.zw;

	vec4 color =  vec4(uv, 0.0, 1.0);

	const uint MaxDrawCount = _View.drawCount;

	const uint earlyDrawCount = drawCounts[0];
	const uint lateDrawCount = drawCounts[1];

	uint drawCount = earlyDrawCount + lateDrawCount;

	bool insideBounds = false;
	bool frustumCulled = drawCount < MaxDrawCount;
	uint culledDraws = 0;
	
#if 1
	for(uint i = 0; i < MaxDrawCount; ++i)
	{
		const MeshDraw meshDraw = draws[i];
		const mat4 worldMatrix = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

		const Mesh mesh = meshes[meshDraw.meshIndex];
		vec4 boundingSphere = mesh.boundingSphere;
		// View space
		boundingSphere.xyz = (_View.viewMatrix * worldMatrix * vec4(boundingSphere.xyz, 1.0f)).xyz;
		vec3 scale = GetScaleFromWorldMatrix(worldMatrix);
		boundingSphere.w *= scale.x; // just uniform scale

	#if 0
		vec4 aabb = vec4(0.0);
		bool projected = GetAxisAlignedBoundingBox(boundingSphere, -_View.zNearFar.x, _View.projMatrix, aabb);
		if (projected)
		{
			if (InsideBounds(fragUV, aabb))
			{
				color.rgb = DebugColors[i & 7];
				insideBounds = true;
				break;
			}
		}

	#else
		frustumCulled = FrustumCull(boundingSphere);
		culledDraws += frustumCulled ? 1 : 0;
	#endif
	}

	color.rgb = DebugColors[culledDraws & 7];

#endif

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
