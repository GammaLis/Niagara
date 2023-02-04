#version 450

#define USE_SUBGROUP 0

#define GROUP_SIZE 32

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#if USE_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot	: require
#endif


layout (binding = 0) readonly buffer Meshes
{
	Mesh meshes[];
};

layout (binding = 1) readonly buffer Draws
{
	MeshDraw draws[];
};

layout (binding = 2) writeonly buffer DrawCommands
{
	MeshDrawCommand drawCommands[];
};

layout (binding = 3) buffer DrawCommandCount
{
	uint drawCommandCount;
};


layout (local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint globalThreadId = gl_GlobalInvocationID.x;

	if (globalThreadId >= _View.drawCount)
		return;

	const MeshDraw meshDraw = draws[globalThreadId];
	const mat4 worldMatrix = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	const Mesh mesh = meshes[meshDraw.meshIndex];
	vec4 boundingSphere = mesh.boundingSphere;
	// View space
	boundingSphere.xyz = (_View.viewMatrix * worldMatrix * vec4(boundingSphere.xyz, 1.0f)).xyz;
	vec3 scale = GetScaleFromWorldMatrix(worldMatrix);
	boundingSphere.w *= scale.x; // just uniform scale

	// Frustum cull
	bool bVisible = !FrustumCull(boundingSphere);

#if USE_SUBGROUP
	uvec4 ballot = subgroupBallot(bVisible);
	uint visibleCount = subgroupBallotBitCount(ballot);
	if (visibleCount == 0)
		return;

	uint drawIndex = 0;
	if (gl_SubgroupInvocationID == 0)
	{
		drawIndex = atomicAdd(drawCommandCount, visibleCount);	
	}
	drawIndex = subgroupBroadcastFirst(drawIndex);
	uint subgroupIndex = subgroupBallotExclusiveBitCount(ballot);
	drawIndex += subgroupIndex;
#endif

	if (bVisible)
	{
#if !USE_SUBGROUP
		uint drawIndex = atomicAdd(drawCommandCount, 1);
#endif

		// Choose one lod
		float lodDistance = log2(max(1, (distance(boundingSphere.xyz, vec3(0.0f)) - boundingSphere.w)));
		uint lodIndex = clamp(int(lodDistance), 0, int(mesh.lodCount) - 1);

		MeshLod meshLod = mesh.lods[lodIndex];
		// DEBUG::
		// meshLod = mesh.lods[max(0, int(mesh.lodCount) - 1)];
		
		MeshDrawCommand drawCommand;

		drawCommand.drawId = globalThreadId;

		drawCommand.indexCount = meshLod.indexCount;
	    drawCommand.instanceCount = 1;
	    drawCommand.firstIndex = meshLod.indexOffset;
	    drawCommand.vertexOffset = mesh.vertexOffset;
	    drawCommand.firstInstance = 0;

	#if defined(USE_NV_MESH_SHADER)
	    drawCommand.taskCount = (meshLod.meshletCount + TASK_GROUP_SIZE-1) / TASK_GROUP_SIZE;
	    drawCommand.firstTask = meshLod.meshletOffset / TASK_GROUP_SIZE;

	#elif defined(USE_EXT_MESH_SHADER)
		drawCommand.taskOffset  = meshLod.meshletOffset;
		drawCommand.taskCount 	= meshLod.meshletCount;
		drawCommand.groupCountX = (meshLod.meshletCount + TASK_GROUP_SIZE-1) / TASK_GROUP_SIZE;
		drawCommand.groupCountY = 1;
		drawCommand.groupCountZ = 1;
	#endif

	    drawCommands[drawIndex] = drawCommand;
	}	
}
