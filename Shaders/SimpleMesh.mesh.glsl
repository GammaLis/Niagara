// Ref: https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_mesh_shader.adoc
// Ref: https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_mesh_shader.txt

#version 450 core

// Settings

#define USE_EXT_MESH_SHADER 1
#define USE_PER_PRIMITIVE 0

#define DEBUG 0
#define CULL 1

/// Extensions
#extension GL_EXT_mesh_shader	: require
// #define GL_EXT_mesh_shader 1

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#extension GL_ARB_shader_draw_parameters: require // gl_DrawIDARB

#if 0
#if USE_8BIT_16BIT_EXTENSIONS
#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
#endif
#endif

#ifndef MESH_GROUP_SIZE
#define MESH_GROUP_SIZE 32
#endif


layout (std430, binding = DESC_VERTEX_BUFFER) readonly buffer Vertices
{
	Vertex vertices[];
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

layout (std430, binding = DESC_MESHLET_DATA_BUFFER) readonly buffer MeshletData
{
	uint meshletData[];
};

layout (location = 0) out Interpolant
{
	vec3 outNormal;
	vec2 outUV;
} OUT[];

taskPayloadSharedEXT TaskPayload payload;


/**
 * >> GL_EXT_mesh_shader
 * gl_NumWorkGroups
 * gl_WorkGroupSize
 * gl_WorkGroupID
 * gl_LocalInvocationID
 * gl_GlobalInvocationID
 * gl_LocalInvocationIndex
 * gl_PrimitivePointIndicesEXT
 * gl_PrimitiveLineIndicesEXT
 * gl_PrimitiveTriangleIndicesEXT
 * gl_Position
 * gl_PointSize
 * gl_ClipDistance
 * gl_CullDistance
 * gl_PrimitiveID
 * gl_Layer
 * gl_ViewportIndex
 * gl_CullPrimitiveEXT
 * gl_DrawID
 *
 * EmitMeshTasksEXT
 * SetMeshOutputsEXT
 */

/**
	// workgroup dimensions
	in    uvec3 gl_NumWorkGroups;
	const uvec3 gl_WorkGroupSize;

	// workgroup and invocation IDs
	in uvec3 gl_WorkGroupID;
	in uvec3 gl_LocalInvocationID;
	in uvec3 gl_GlobalInvocationID;
	in uint  gl_LocalInvocationIndex;

	// write only access
	out uint  gl_PrimitivePointIndicesEXT[];
	out uvec2 gl_PrimitiveLineIndicesEXT[];
	out uvec3 gl_PrimitiveTriangleIndicesEXT[];

	// write only access
	out gl_MeshPerVertexEXT {
	vec4  gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
	float gl_CullDistance[];
	} gl_MeshVerticesEXT[];

	// write only access
	perprimitiveEXT out gl_MeshPerPrimitiveEXT {
	int  gl_PrimitiveID;
	int  gl_Layer;
	int  gl_ViewportIndex;
	bool gl_CullPrimitiveEXT;
	int  gl_PrimitiveShadingRateEXT;
	} gl_MeshPrimitivesEXT[];

	* The output variable gl_CullPrimitiveEXT is only available in the
    mesh language. When set to true, it marks that this primitive should
    be culled. If not written to, it defaults to false.
 */

/**
	void SetMeshOutputsEXT(uint vertexCount,
                           uint primitiveCount)
	Sets the actual output size of the primitives and vertices that this mesh shader workgroup will emit upon
	completion. The vertexCount argument must be less or equal than the provided `max_vertices` identifier and
	the primitiveCount argument must be less or equal to `max_primitives`.

	void memoryBarrierShared()

	void groupMemoryBarrier()
 */

#if USE_PER_PRIMITIVE
// layout (location = 2)
// perprimitiveEXT out vec3 triangleNormals[];
#endif

#if CULL
shared vec3 sh_VertexClip[MAX_VERTICES];
#endif


// Set the number of threads per workgroup (always one-dimensional)
layout (local_size_x = MESH_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
// Mesh shader
// The primitive type (points, lines, triangles)
layout (triangles) out;
// `gl_PrimitivePointIndicesEXT`	- points
// `gl_PrimitiveLineIndicesEXT`		- lines
// `gl_PrimitiveTriangleIndicesEXT`	- triangles
// Maximum allocation size for each meshlet
layout (max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;
void main()
{
	const uint localThreadIndex = gl_LocalInvocationIndex;

	const uint meshletIndex = payload.meshletIndices[gl_WorkGroupID.x];

#if 0
	const MeshDrawCommand meshDrawCommand = drawCommands[gl_DrawIDARB];
	const MeshDraw meshDraw = draws[meshDrawCommand.drawId];
#else
	const MeshDraw meshDraw = draws[payload.drawId];
#endif

	const mat4 worldMat = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	const uint vertexCount = uint(meshlets[meshletIndex].vertexCount);
	const uint triangleCount = uint(meshlets[meshletIndex].triangleCount);
	const uint indexCount = triangleCount * 3;

	const uint vertexOffset = meshlets[meshletIndex].vertexOffset;
	const uint indexOffset = vertexOffset + vertexCount;

#if DEBUG
	vec3 meshletColor = IntToColor(meshletIndex);
#endif

	SetMeshOutputsEXT(vertexCount, triangleCount);

	// Vertices
	for (uint i = localThreadIndex; i < vertexCount; i += MESH_GROUP_SIZE)
	{
		uint vi = meshletData[vertexOffset + i] + meshDraw.vertexOffset;

		vec3 posOS = vec3(vertices[vi].px, vertices[vi].py, vertices[vi].pz);

		// position.z = position.z * 0.5 + 0.5;
		vec4 position = _View.viewProjMatrix * worldMat * vec4(posOS, 1.0);
		vec3 normal = vec3(uint(vertices[vi].nx), uint(vertices[vi].ny), uint(vertices[vi].nz)) / 127.0 - 1.0;
		vec2 texcoord = vec2(vertices[vi].s, vertices[vi].t);

		gl_MeshVerticesEXT[i].gl_Position = position;
	#if !DEBUG
		OUT[i].outNormal = normal;
	#else
		OUT[i].outNormal = meshletColor;
	#endif
		OUT[i].outUV = texcoord;

	#if CULL
		sh_VertexClip[i] = vec3(position.xy / position.w, position.w);
	#endif
	}

#if CULL
	barrier(); // memoryBarrierShared();
#endif

	// Primitives
	for (uint i = localThreadIndex; i < triangleCount; i += MESH_GROUP_SIZE)
	{
		uint indices = meshletData[indexOffset + i];
		uint i0 = indices & 0xFF, i1 = (indices >>  8) & 0xFF, i2 = (indices >> 16) & 0xFF;
		gl_PrimitiveTriangleIndicesEXT[i] = uvec3(i0, i1, i2);

	#if CULL
		bool culled = false;

		vec3 p0 = sh_VertexClip[i0], p1 = sh_VertexClip[i1], p2 = sh_VertexClip[i2];

		vec2 c0 = p0.xy, c1 = p1.xy, c2 = p2.xy;

		// Backfacing culling + small-area culling
		if (_DebugParams.triBackfaceCulling > 0)
		{
			vec2 c01 = c1 - c0, c02 = c2 - c0;
			float area = (c01.x * c02.y - c01.y * c02.x);
			
			culled = culled || area <= 0; // 1e-5;
		}

		// Small primitive culling
		if (_DebugParams.triSmallCulling > 0)
		{
			vec2 bmin = min(c0, min(c1, c2));
			vec2 bmax = max(c0, max(c1, c2));
			
			bmin = (bmin * 0.5 + 0.5) * _View.viewportRect.zw;
			bmax = (bmax * 0.5 + 0.5) * _View.viewportRect.zw;

			const float subpixelPrec = 1.0 / 256.0; // this can be set to 1/2^SubpixelPrecisionBits.	512 256

		#if 0
			// Note: this is slightly imprecise (doesn't fully match hw behavior and is bot too loose and too strict)
			culled = culled || (round(bmin.x - subpixelPrec) == round(bmax.x + subpixelPrec) || round(bmin.y - subpixelPrec) == round(bmax.y + subpixelPrec));
		#else
			// Due to top-left rasterization convention it is probably safe to slightly reduce the tested bbox
			// https://github.com/zeux/niagara.git # e9f3a890af764d765efd544e243452c380b08d50
			culled = culled || (round(bmin.x - subpixelPrec) == round(bmax.x) || round(bmin.y) == round(bmax.y + subpixelPrec));
		#endif
		}

		// culled = culled || (p0.z < 0 && p1.z < 0 && p2.z < 0);

		// The computations above are only valid if all vertices are in front of perspective plane
		culled = culled && (p0.z > 0 && p1.z > 0 && p2.z > 0);

		// TODO: Requires fragment shading rate ??? 
		gl_MeshPrimitivesEXT[i].gl_CullPrimitiveEXT = culled;
	#endif
	}

#if USE_PER_PRIMITIVE
	for (uint i = localThreadId; i < triangleCount; i += MESH_GROUP_SIZE)
	{
	#if 0
		uint index0 = uint(meshlets[meshletIndex].indices[i * 3 + 0]);
		uint index1 = uint(meshlets[meshletIndex].indices[i * 3 + 1]);
		uint index2 = uint(meshlets[meshletIndex].indices[i * 3 + 2]);

		uint vi0 = meshlets[meshletIndex].vertices[index0];
		uint vi1 = meshlets[meshletIndex].vertices[index1];
		uint vi2 = meshlets[meshletIndex].vertices[index2];

		vec3 p0 = vec3(vertices[vi0].px, vertices[vi0].py, vertices[vi0].pz);		
		vec3 p1 = vec3(vertices[vi1].px, vertices[vi1].py, vertices[vi1].pz);		
		vec3 p2 = vec3(vertices[vi2].px, vertices[vi2].py, vertices[vi2].pz);

		vec3 n = normalize(cross(p1 - p0, p2 - p0));

		triangleNormals[i] = n;
	#endif
	}
#endif
}

// https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_mesh_shader.txt
/**
 * Mesh Processor
 * The mesh processor is a programmable unit that operates in conjunction with the task processor to
 * produce a collection of primitives that will be processed by subsequent stages of the graphics pipeline.
 * A mesh shader has access to many of the same resources as fragment and other shader processors, including
 * textures, buffers, image variables, and atomic counters.  The only inputs available to the mesh shader are
 * variables identifying the specific workgroup and invocation and any outputs written to task memory by
 * the task shader that spawned the mesh shader's workgroup. 
 * A mesh shader operates on a group of work items called a workgroup. An invocation within a workgroup may 
 * share data with other members of the same workgroup through shared variables and issue memory and control
 * barriers to synchronize with other members of the same workgroup.
 */

/**
 * shared - compute, task and mesh shader only; variable storage is shared across all work items in a local workgrup
 * taskPayloadSharedEXT - task and mesh shader only; storage that is visible to task shader work items and 
 the mesh shader work items they spawn and is shared across all work items in a local workgroup
 * perprimitiveEXT - mesh shader outputs with per-primitive instances
 */
