#version 450

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#define GROUP_SIZE 32
#define TASK_GROUP_SIZE 32

layout (binding = 0) readonly buffer Draws
{
	MeshDraw draws[];
};

layout (binding = 1) writeonly buffer DrawCommands
{
	MeshDrawCommand drawCommands[];
};

bool FrustumCull(vec4 boundingSphere)
{
	bool bVisible = true;
	for (uint i = 0; i < 6; ++i)
	{
		bVisible = dot(_View.frustumPlanes[i], vec4(boundingSphere.xyz, 1.0)) < boundingSphere.w;
		if (!bVisible)
			break;
	}

	return bVisible;
}


layout (local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint globalThreadId = gl_GlobalInvocationID.x;

	const MeshDraw meshDraw = draws[globalThreadId];
	const mat4 worldMatrix = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	vec4 boundingSphere = meshDraw.boundingSphere;
	// View space
	boundingSphere.xyz = (_View.viewMatrix * worldMatrix * vec4(boundingSphere.xyz, 1.0f)).xyz;
	vec3 scale = GetScaleFromWorldMatrix(worldMatrix);
	boundingSphere.w *= scale.x; // just uniform scale

	// Frustum cull
	bool bVisible = FrustumCull(boundingSphere);

	MeshDrawCommand drawCommand;

	drawCommand.indexCount = meshDraw.indexCount;
    drawCommand.instanceCount = bVisible ? 1 : 0;
    drawCommand.firstIndex = meshDraw.indexOffset;
    drawCommand.vertexOffset = meshDraw.vertexOffset;
    drawCommand.firstInstance = 0;

    drawCommand.taskCount = bVisible ? (meshDraw.meshletCount + TASK_GROUP_SIZE-1) / TASK_GROUP_SIZE : 0;
    drawCommand.firstTask = 0;

    drawCommands[globalThreadId] = drawCommand;
}