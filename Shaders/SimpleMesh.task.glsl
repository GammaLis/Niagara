#version 450 core

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#extension GL_NV_mesh_shader	: require
// #define GL_EXT_mesh_shader 1

// Settings
#define GROUP_SIZE 32

#define DEBUG 0
#define CULL 1

layout (std430, binding = 0) buffer Vertices
{
	Vertex vertices[];
};

layout (std430, binding = 1) buffer Meshlets
{
	Meshlet meshlets[];
};

out taskNV block 
{
	uint meshletIndices[GROUP_SIZE];
};

bool ConeCull(vec4 cone, vec3 viewDir)
{
	float VdotCone = dot(-viewDir, cone.xyz);
	float threshold = sqrt(1.0 - cone.w * cone.w);
	return cone.w < 0 ? VdotCone > threshold : VdotCone < -threshold;
}

shared uint sh_MeshletCount;

// Set the number of threads per workgroup (always one-dimensional)
layout (local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
	const uint groupId = gl_WorkGroupID.x;
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint meshletIndex = groupId * GROUP_SIZE + localThreadId;

#if CULL
	if (localThreadId == 0)
		sh_MeshletCount = 0;

	// Sync
	memoryBarrierShared();

	if (!ConeCull(meshlets[meshletIndex].cone, vec3(0, 0, 1)))
	{
		uint index = atomicAdd(sh_MeshletCount, 1);
		meshletIndices[localThreadId] = meshletIndex;
	}

	// Sync
	memoryBarrierShared();

	if (localThreadId == 0)
		gl_TaskCountNV = sh_MeshletCount;

#else
	meshletIndices[localThreadId] = meshletIndex;
	if (localThreadId == 0)
		gl_TaskCountNV = GROUP_SIZE;
#endif
}
