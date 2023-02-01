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

#extension GL_ARB_shader_draw_parameters	: require // gl_DrawIDARB

// Settings
#define GROUP_SIZE 32

#define DEBUG 0
#define CULL 1

layout (std430, binding = DESC_VERTEX_BUFFER) readonly buffer Vertices
{
	Vertex vertices[];
};

layout (std430, binding = DESC_MESH_BUFFER) readonly buffer Meshes
{
	Mesh meshes[];
};

layout (std430, binding = DESC_DRAW_DATA_BUFFER) readonly buffer Draws
{
	MeshDraw draws[];
};

layout (std430, binding = DESC_DRAW_COMMAND_BUFFER) readonly buffer DrawCommands
{
	MeshDrawCommand drawCommands[];
};

layout (std430, binding = DESC_MESHLET_BUFFER) readonly buffer Meshlets
{
	Meshlet meshlets[];
};

out taskNV block 
{
	uint meshletIndices[GROUP_SIZE];
};

/**
 * >> MeshOptimizer
 * For backface culling with orthographic projection, use the following formula to reject backfacing clusters:
 *   dot(view, cone_axis) >= cone_cutoff
 *
 * For perspective projection, you can the formula that needs cone apex in addition to axis & cutoff:
 *   dot(normalize(cone_apex - camera_position), cone_axis) >= cone_cutoff
 *
 * Alternatively, you can use the formula that doesn't need cone apex and uses bounding sphere instead:
 *   dot(normalize(center - camera_position), cone_axis) >= cone_cutoff + radius / length(center - camera_position)
 * or an equivalent formula that doesn't have a singularity at center = camera_position:
 *   dot(center - camera_position, cone_axis) >= cone_cutoff * length(center - camera_position) + radius
 *
 * The formula that uses the apex is slightly more accurate but needs the apex; if you are already using bounding sphere
 * to do frustum/occlusion culling, the formula that doesn't use the apex may be preferable (for derivation see
 * Real-Time Rendering 4th Edition, section 19.3).
 */

bool ConeCull(vec4 cone, vec3 viewDir)
{
	float VdotCone = dot(viewDir, cone.xyz);
	float threshold = sqrt(1.0 - cone.w * cone.w);
	// FIXME:
	// return cone.w < 0 ? VdotCone > threshold : VdotCone < -threshold;
	// return cone.w > 0 && VdotCone < -threshold;
	return VdotCone >= cone.w;
}

bool ConeCull_ConeApex(vec4 cone, vec3 coneApex, vec3 camPos)
{
	return dot(normalize(coneApex - camPos), cone.xyz) >= cone.w;
}

bool ConeCull_BoundingSphere(vec4 cone, vec4 boundingSphere, vec3 camPos)
{
	vec3 sphereToCamVec = boundingSphere.xyz - camPos;
	return dot(cone.xyz, sphereToCamVec) >= cone.w * length(sphereToCamVec) + boundingSphere.w;
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
	const uint localThreadIdStart = groupId * GROUP_SIZE;

	const MeshDrawCommand meshDrawCommand = drawCommands[gl_DrawIDARB];
	const MeshDraw meshDraw = draws[meshDrawCommand.drawId];
	
	const uint meshletIndex = localThreadIdStart + localThreadId;

	const mat4 worldMat = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	vec3 viewDir = vec3(0, 0, 1);

#if CULL

#if USE_SUBGROUP

	bool accept = false;

	// if (meshletIndex < meshDraw.meshletCount + meshDraw.meshletOffset)
	{
		vec4 cone = meshlets[meshletIndex].cone;
		cone = vec4(mat3(worldMat) * cone.xyz, cone.w);

	#if 0
		accept = !ConeCull(cone, viewDir);
	#elif 0
		vec3 coneApex = meshlets[meshletIndex].coneApex.xyz;
		coneApex = (worldMat * vec4(coneApex, 1.0)).xyz;
		accept = !ConeCull_ConeApex(cone, coneApex, _View.camPos);
	#elif 1
		vec4 boundingSphere = meshlets[meshletIndex].boundingSphere;
		boundingSphere.xyz = (worldMat * vec4(boundingSphere.xyz, 1.0)).xyz;
		accept = !ConeCull_BoundingSphere(cone, boundingSphere, _View.camPos);
	#endif
	}
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

	// if (meshletIndex < meshDraw.meshletCount + meshDraw.meshletOffset)
	{
		vec4 cone = meshlets[meshletIndex].cone;
		cone = vec4(mat3(worldMat) * cone.xyz, cone.w);

		if (!ConeCull(cone, viewDir))
		{
			uint index = atomicAdd(sh_MeshletCount, 1);
			meshletIndices[index] = meshletIndex;
		}
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
