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

layout (binding = 5) uniform sampler2D depthPyramid;


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

	color.rgb = vec3(0.0);

	const uint MaxDrawCount = _View.drawCount;

	const uint earlyDrawCount = drawCounts[0];
	const uint lateDrawCount = drawCounts[1];

	uint drawCount = earlyDrawCount + lateDrawCount;

	bool insideBounds = false;
	vec4 bounds = vec4(0.0);
	vec4 cachedSphere = vec4(0.0);

	bool culled = drawCount < MaxDrawCount;
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

	#if 1
		vec4 aabb = vec4(0.0);
		bool projected = GetAxisAlignedBoundingBox(boundingSphere, -_View.zNearFar.x, _View.projMatrix, aabb);
		if (projected)
		{
			if (InsideBounds(uv, aabb))
			{
				color.rgb = DebugColors[(i+1) & 7];
				bounds = aabb;
				cachedSphere = boundingSphere;
				insideBounds = true;
				// break;
			}
		}

	#elif 0
		// Frustum culling
		culled = FrustumCull(boundingSphere);
		culledDraws += culled ? 1 : 0;
	#endif
	}

#if 0
	// Occlusion culling
	if (insideBounds)
	{
		float w = (bounds.z - bounds.x) * _View.depthPyramidSize.x;
		float h = (bounds.w - bounds.y) * _View.depthPyramidSize.y;
		vec2  c = (bounds.xy + bounds.zw) * 0.5;

		vec2 depthPyramidRatio = _View.depthPyramidSize.xy / max(vec2(1.0), _View.depthPyramidSize.zw);
		vec2 uv = c * depthPyramidRatio;

		float level = floor(log2(max(w, h) * 0.8)); // 0.8 - debug value

		// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
		float depth = textureLod(depthPyramid, uv, level).x;
		float sphereDepth = ConvertToDeviceZ(cachedSphere.z + cachedSphere.w);
		culled = depth > sphereDepth;

		color.rgb = culled ? vec3(0.2, 1.0, 1.0) : color.rgb;
	}

#elif 1
	culledDraws = MaxDrawCount - drawCount;
	color.rgb = DebugColors[culledDraws & 7];
#endif
	
#endif

#if 0
	vec2 hizUV = uv * _View.depthPyramidSize.xy / _View.depthPyramidSize.zw;
	float depth = textureLod(DepthPyramid, hizUV, 0.0).x;
	color.rgb = vec3(depth);
#endif

	outColor = color;
}
