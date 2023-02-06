#version 450

#define USE_SUBGROUP 0

#define GROUP_SIZE 64

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#if USE_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot	: require
#endif


layout (push_constant) uniform PushConstants
{
	uint pass;
} _States;


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

layout (binding = 4) buffer DrawVisibilities
{
	uint drawVisibilities[];
};

layout (binding = 5) uniform sampler2D depthPyramid;

// View space occlusion culling
bool OcclusionCull(vec4 boundingSphere)
{
	bool culled = false;

	vec4 aabb = vec4(0, 0, 1, 1);
	bool valid = GetAxisAlignedBoundingBox(boundingSphere, -_View.zNearFar.x, _View.projMatrix, aabb);
    if (valid) 
    {
    	float w = (aabb.z - aabb.x) * _View.depthPyramidSize.x;
    	float h = (aabb.w - aabb.y) * _View.depthPyramidSize.y;
    	vec2  c = (aabb.xy + aabb.zw) * 0.5;

    	vec2 depthPyramidRatio = _View.depthPyramidSize.xy / max(vec2(1.0), _View.depthPyramidSize.zw);
    	vec2 uv = c * depthPyramidRatio;

    	float level = floor(log2(max(w, h)));

		// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
		float depth = textureLod(depthPyramid, c, level).x;
		float sphereDepth = ConvertToDeviceZ(boundingSphere.z + boundingSphere.w);
		culled = depth > sphereDepth;
    }

	return culled;
}


layout (local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main()
{
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint globalThreadId = gl_GlobalInvocationID.x;

	if (globalThreadId >= _View.drawCount)
		return;

	const uint pass = _States.pass;
	const uint drawVisibility = drawVisibilities[globalThreadId];

	// In early pass, dont't process draws that were not visible last frame
	if (pass == 0 && drawVisibility == 0)
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
	bool bVisible = true;

	if (_DebugParams.drawFrustumCulling > 0)
		bVisible = bVisible && !FrustumCull(boundingSphere);

	// Only doing oc in late pass
	if (_DebugParams.drawOcclusionCulling > 0 && pass > 0)
		bVisible = bVisible && !OcclusionCull(boundingSphere);

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

	if (bVisible && (_States.pass == 0 || drawVisibility == 0))
	{
#if !USE_SUBGROUP
		uint drawIndex = atomicAdd(drawCommandCount, 1);
#endif

		// Choose one lod
		float lodDistance = log2(max(1, length(boundingSphere.xyz) - boundingSphere.w));
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

	// Update draw visibilities in late pass
	if (pass > 0)
		drawVisibilities[globalThreadId] = bVisible ? 1 : 0;
}
