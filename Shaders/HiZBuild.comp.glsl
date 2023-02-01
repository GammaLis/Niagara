#version 450

#extension GL_GOOGLE_include_directive	: require

#define USE_REVERSED_Z 1

#define GROUP_SIZE 8


/**
 * layout (push_constant) uniform BlockName {
 * 	 int m0;
 * 	 float m1;
 * 	 ...
 * } InstanceName; // optional instance name
 */
layout (push_constant) uniform Constants
{
	vec4 srcSize;
	vec2 viewportMaxBoundUV;
} _Constants;

// Combined image sampler
layout (binding = 0) uniform sampler2D inputImage;

layout (binding = 1) uniform writeonly image2D outputImage;


float GetFurthestDepth(vec4 depths)
{
#if USE_REVERSED_Z
	return min(min(depths.x, depths.y), min(depths.z, depths.w));
#else
	return max(max(depths.x, depths.y), max(depths.z, depths.w));
#endif
}

float GetClosestDepth(vec4 depths)
{
#if USE_REVERSED_Z
	return max(max(depths.x, depths.y), max(depths.z, depths.w));
#else
	return min(min(depths.x, depths.y), min(depths.z, depths.w));
#endif
}

vec4 Gather4(sampler2D texSampler, vec2 bufferUV)
{
	vec2 uv = min(bufferUV + vec2(-0.25f) * _Constants.srcSize.zw, _Constants.viewportMaxBoundUV.xy - _Constants.srcSize.zw);

	vec4 depths = textureGather(texSampler, uv, 0);

	return depths;
}


layout (local_size_x = GROUP_SIZE, local_size_y = GROUP_SIZE, local_size_z = 1) in;
void main()
{
	const uvec2 groupId = gl_WorkGroupID.xy;
	const uint localThreadIndex = gl_LocalInvocationIndex;

#if 0
	// TODO: UE - HZB.usf:  InitialTilePixelPositionForReduction2x2
	const uvec2 localThreadId = uvec2(localThreadIndex % GROUP_SIZE, localThreadIndex / GROUP_SIZE);
	const uvec2 globalThreadId = gl_WorkGroupID.xy * GROUP_SIZE + localThreadId;
#else
	const uvec2 globalThreadId = gl_GlobalInvocationID.xy;
#endif

	vec2 bufferUV = (globalThreadId + 0.5f) * 2 * _Constants.srcSize.zw;
	vec4 depths = Gather4(inputImage, bufferUV);
	float zFurthest = GetFurthestDepth(depths);

	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(zFurthest, 0, 0, 0));
}
