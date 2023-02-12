#version 450 core

/// Settings
#ifndef TASK_GROUP_SIZE
#define TASK_GROUP_SIZE 32
#endif

// TODO: Not used now
#define USE_SUBGROUP 0
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


layout (push_constant) uniform PushConstants
{
	uint pass;
} _States;


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

layout (std430, binding = DESC_MESHLET_VISIBILITY_BUFFER) buffer MeshletVisibilities
{
	uint meshletVisibilities[];
};

layout (binding = DESC_DEPTH_PYRAMID) uniform sampler2D depthPyramid;

taskPayloadSharedEXT TaskPayload payload;


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
	const uint globalThreadId = gl_GlobalInvocationID.x;
	
	const MeshDrawCommand meshDrawCommand = drawCommands[gl_DrawIDARB];
	const MeshDraw meshDraw = draws[meshDrawCommand.drawId];
	
	const uint meshletOffset = meshDrawCommand.taskOffset;
	const uint meshletCount = meshDrawCommand.taskCount;
	const uint meshletMaxIndex = meshletOffset + meshletCount;
	const uint meshletIndex =  meshletOffset + globalThreadId;

	const mat4 worldMatrix = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	vec3 viewDir = vec3(0, 0, 1);

	const uint drawVisibility = meshDrawCommand.drawVisibility;
	// For oc
	const uint meshletVisibilityIndex = globalThreadId + meshDraw.meshletVisibilityOffset;
	const uint meshletVisibility = meshletVisibilities[meshletVisibilityIndex];

	const uint pass = _States.pass;

	payload.drawId = meshDrawCommand.drawId;

#if CULL

#if USE_SUBGROUP

	bool accept = false;

	if (meshletIndex < meshletMaxIndex)
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
	/**
	 * https://docs.gl/sl4/barrier
	 * A barrier() affects control flow but only synchronizes memory accesses to shared variables and tessellation control output variables. 
	 * For other memory accesses, it does not ensure that values written by one invocation prior to a given static instance of barrier() 
	 * can be safely read by other invocations after their call to the same static instance of barrier(). To achieve this requires the use of
	 * both barrier() and a memory barrier.
	 * ==> If you are only using barriers for shared variables, barrier() is sufficient. If you are using them for "other memory accesses", 
	 * then barrier() is not sufficient.
	 */
	barrier(); // memoryBarrierShared();

	bool valid = meshletIndex < meshletMaxIndex;

	bool accept = valid && (_DebugParams.meshletOcclusionCulling == 0 || pass > 0 || meshletVisibility > 0);

	bool skip = pass > 0 && drawVisibility > 0 && meshletVisibility > 0 /* && _DebugParams.meshletOcclusionCulling > 0 */;

	// Culling
	{
		// TODO: View space cone culling ?
		// World space cone culling
		vec4 cone = meshlets[meshletIndex].cone;
		cone = vec4(mat3(worldMatrix) * cone.xyz, cone.w);

		vec4 boundingSphere = meshlets[meshletIndex].boundingSphere;
		boundingSphere.xyz = (worldMatrix * vec4(boundingSphere.xyz, 1.0)).xyz;
		vec3 scale = GetScaleFromWorldMatrix(worldMatrix);
		boundingSphere.w *= scale.x; // just uniform scale

		if (_DebugParams.meshletConeCulling > 0)
			accept = accept && !ConeCull_BoundingSphere(cone, boundingSphere, _View.camPos);

		boundingSphere.xyz = (_View.viewMatrix * vec4(boundingSphere.xyz, 1.0f)).xyz;
		
		// View space frustum culling
		if (_DebugParams.meshletFrustumCulling > 0)
			accept = accept && !FrustumCull(boundingSphere);

		// View space occlusion culling
		if (_DebugParams.meshletOcclusionCulling > 0 && pass > 0)
			accept = accept && !OcclusionCull(depthPyramid, boundingSphere);
	}

	if (accept && !skip)
	{
		uint index = atomicAdd(sh_MeshletCount, 1);
		payload.meshletIndices[index] = meshletIndex;
	}
	
	// Sync
	barrier(); // memoryBarrierShared();

	if (localThreadId == 0)
	{
		uint groupCountX = sh_MeshletCount;
      	uint groupCountY = 1;
      	uint groupCountZ = 1;
		EmitMeshTasksEXT(groupCountX, groupCountY, groupCountZ);
	}

	if (pass > 0 && valid)
		meshletVisibilities[meshletVisibilityIndex] = accept ? 1 : 0;

#endif

#else
	if (meshletIndex < meshletMaxIndex)
		payload.meshletIndices[localThreadId] = meshletIndex;

	if (localThreadId == 0)
	{
		uint groupCountX = min(TASK_GROUP_SIZE, meshletCount - groupId * TASK_GROUP_SIZE); // localThreadIdStart
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
