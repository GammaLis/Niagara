#version 450 core

/// Settings
#ifndef TASK_GROUP_SIZE
#define TASK_GROUP_SIZE 32
#endif

#define USE_SUBGROUP 1
#define USE_EXT_MESH_SHADER 1

#define DEBUG 0
#define CULL 1

/// Extensions
#extension GL_EXT_mesh_shader	: require
// #define GL_EXT_mesh_shader 1

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#if USE_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot	: require
#endif

#extension GL_ARB_shader_draw_parameters	: require // gl_DrawIDARB


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

taskPayloadSharedEXT TaskPayload payload;

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

/**
 	// workgroup dimensions
	in    uvec3 gl_NumWorkGroups;
	const uvec3 gl_WorkGroupSize;

	// workgroup and invocation IDs
	in uvec3 gl_WorkGroupID;
	in uvec3 gl_LocalInvocationID;
	in uvec3 gl_GlobalInvocationID;
	in uint  gl_LocalInvocationIndex;
 */

/**
 	void EmitMeshTasksEXT(uint groupCountX,
                          uint groupCountY,
                          uint groupCountZ)
	Defines the grid size of subsequent mesh shader workgroups to generate upon completion of 
	the task shader invocation group that called this function and exists the shader. These mesh shader workgroups
	will have access to the data that was written to a variable with the taskPayloadSharedEXT qualifier.
	The function call implies a `barrier()`. The arguments are taken from the first invocation in each workgroup.
	Any invocation must call this function exactly once and under uniform control flow, otherwise behavior is undefined,
 */

// Set the number of threads per workgroup (always one-dimensional)
layout (local_size_x = TASK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
	const uint groupId = gl_WorkGroupID.x;
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint localThreadIdStart = groupId * TASK_GROUP_SIZE;

	const MeshDrawCommand meshDrawCommand = drawCommands[gl_DrawIDARB];
	const MeshDraw meshDraw = draws[meshDrawCommand.drawId];
	
	const uint meshletOffset = meshDrawCommand.taskOffset;
	const uint meshletCount = meshDrawCommand.taskCount;
	const uint meshletMaxIndex = meshletOffset + meshletCount;
	const uint meshletIndex = localThreadIdStart + localThreadId + meshletOffset;

	const mat4 worldMat = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	vec3 viewDir = vec3(0, 0, 1);

#if CULL

#if USE_SUBGROUP

	bool accept = false;

	// if (meshletIndex < meshletMaxIndex)
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
		payload.meshletIndices[index] = meshletIndex; // localThreadId
	}

	uint count = subgroupBallotBitCount(ballot);

	if (localThreadId == 0)
	{
		uint groupCountX = count;
      	uint groupCountY = 1;
      	uint groupCountZ = 1;
		EmitMeshTasksEXT(groupCountX, groupCountY, groupCountZ);
	}

#else
	if (localThreadId == 0)
		sh_MeshletCount = 0;

	// Sync
	memoryBarrierShared();

	// if (meshletIndex < meshletCount + meshletOffset)
	{
		vec4 cone = meshlets[meshletIndex].cone;
		cone = vec4(mat3(worldMat) * cone.xyz, cone.w);

		if (!ConeCull(cone, viewDir))
		{
			uint index = atomicAdd(sh_MeshletCount, 1);
			payload.meshletIndices[index] = meshletIndex;
		}
	}
	
	// Sync
	memoryBarrierShared();

	if (localThreadId == 0)
	{
		uint groupCountX = sh_MeshletCount;
      	uint groupCountY = 1;
      	uint groupCountZ = 1;
		EmitMeshTasksEXT(groupCountX, groupCountY, groupCountZ);
	}
#endif

#else
	if (meshletIndex < meshletMaxIndex)
		payload.meshletIndices[localThreadId] = meshletIndex;

	if (localThreadId == 0)
	{
		uint groupCountX = min(TASK_GROUP_SIZE, meshletCount - localThreadIdStart);
      	uint groupCountY = 1;
      	uint groupCountZ = 1;
		EmitMeshTasksEXT(groupCountX, groupCountY, groupCountZ);
	}
#endif
}

// https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_mesh_shader.txt
/**
 * Task Processor
 * The task processor is a programmable unit that operates in conjunction with the mesh processsor to 
 * produce a collection of primitives that will be processed by subsequent stages of the graphics pipeline.
 * A task shader has access to many of the same resources as fragment and other shader processors, including 
 * textures, buffers, image variables, and atomic counters. The task shader has no fixed-function inputs 
 * other than variables identifying the specific workgroup and invocation; any vertex attributes or other
 * data required by the task shader must be fetched from memory. The only fixed output of the task shader is
 * a task count, identifying the number of mesh shader workgroups to spawn. The task shader can write addtional
 * outputs to task memory, which can be read by all of the mesh shader workgroups it spawns.
 * A task shader operates on a group of work items called a workgroup.
 */
