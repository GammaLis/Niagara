#version 450 core

/// Extensions

#define USE_SUBGROUP 1

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#extension GL_NV_mesh_shader	: require
// #define GL_EXT_mesh_shader 1

#if USE_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot	: require
#endif

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
	float VdotCone = dot(viewDir, cone.xyz);
	float threshold = sqrt(1.0 - cone.w * cone.w);
	// FIXME:
	// return cone.w < 0 ? VdotCone > threshold : VdotCone < -threshold;
	return cone.w > 0 && VdotCone < -threshold;
}

#if !USE_SUBGROUP
shared uint sh_MeshletCount;
#endif

// Set the number of threads per workgroup (always one-dimensional)
layout (local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
	const uint groupId = gl_WorkGroupID.x;
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint meshletIndex = groupId * GROUP_SIZE + localThreadId;

	vec3 viewDir = vec3(0, 0, 1);

#if CULL

#if USE_SUBGROUP
	bool accept = !ConeCull(meshlets[meshletIndex].cone, viewDir);
	uvec4 ballot = subgroupBallot(accept);
	uint index = subgroupBallotExclusiveBitCount(ballot);

	if (accept)
	{
		meshletIndices[index] = meshletIndex; // localThreadId
	}

	uint count = subgroupBallotBitCount(ballot);

	if (localThreadId == 0)
		gl_TaskCountNV = count;

#else
	if (localThreadId == 0)
		sh_MeshletCount = 0;

	// Sync
	memoryBarrierShared();

	if (!ConeCull(meshlets[meshletIndex].cone, viewDir))
	{
		uint index = atomicAdd(sh_MeshletCount, 1);
		meshletIndices[index] = meshletIndex;
	}
	
	// Sync
	memoryBarrierShared();

	if (localThreadId == 0)
		gl_TaskCountNV = sh_MeshletCount;
#endif

#else
	meshletIndices[localThreadId] = meshletIndex;
	if (localThreadId == 0)
		gl_TaskCountNV = GROUP_SIZE;
#endif
}
