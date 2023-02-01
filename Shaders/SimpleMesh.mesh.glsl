#version 450 core

#extension GL_GOOGLE_include_directive	: require
#include "MeshCommon.h"

#extension GL_NV_mesh_shader	: require
// #define GL_EXT_mesh_shader 1

#extension GL_ARB_shader_draw_parameters: require // gl_DrawIDARB

#if 0
#if USE_8BIT_16BIT_EXTENSIONS
#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
#endif
#endif

// Settings
#define GROUP_SIZE 32

#define USE_PER_PRIMITIVE 0
#define USE_PACKED_PRIMITIVE_INDICES_NV 1
#define DEBUG 0

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

in taskNV block
{
	uint meshletIndices[GROUP_SIZE];
};

layout (location = 0) out Interpolant
{
	vec3 outNormal;
	vec2 outUV;
} OUT[];

#if 0

// The actual number of primitives the workgroup outputs (<= max_primitives)
out uint gl_PrimitiveCountNV;
// An index buffer, using list type indices (strips are not supported here)
out uint gl_PrimitiveIndicesNV[]; // [max_primitives * 3 for triangles]

out gl_MeshPerVertexNV {
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
	float gl_CullDistance[];
} gl_MeshVerticesNV[]; // [max_vertices]

// Define your own vertex output blocks as usual
out Interpolant {
	vec2 uv;
} OUT[]; // [max_vertices]

// Special purpose per-primitive outputs
perprimitiveNV out gl_MeshPerPrimitiveNV {
 	int gl_PrimitiveID;
 	int gl_Layer;
 	int gl_ViewportIndex;
 	int gl_ViewportMask[]; // [1]
} gl_MeshPrimitivesNV[]; // [max_primitives]

A typical mesh shader used to render static triangle data might operate in 3 phases.
* Fetches vertex position data and local index data of the primitives that the mesh represents.
* Triangles would be culled and ouptut primitive indices written.
* Other vertex attribtes of the surviving subset of vertices would be loaded and computed. During this process,
the invocations would sometimes work on a per-vertex and sometimes on a per-primitive level.

#endif

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
 * EmitMeshTasksEXT -> OpEmitMeshTasksEXT()
 * SetMeshOutputsEXT -> OpSetMeshOutputsEXT()
 */

#if USE_PER_PRIMITIVE
// layout (location = 2)
// perprimitiveNV out vec3 triangleNormals[];
#endif


// Set the number of threads per workgroup (always one-dimensional)
layout (local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
// Mesh shader
// The primitive type (points, lines, triangles)
layout (triangles) out;
// Maximum allocation size for each meshlet
layout (max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;
void main()
{
	const uint meshletIndex = meshletIndices[gl_WorkGroupID.x];
	const uint localThreadId = gl_LocalInvocationID.x;

	const MeshDrawCommand meshDrawCommand = drawCommands[gl_DrawIDARB];
	const MeshDraw meshDraw = draws[meshDrawCommand.drawId];

	const mat4 worldMat = BuildWorldMatrix(meshDraw.worldMatRow0, meshDraw.worldMatRow1, meshDraw.worldMatRow2);

	const uint vertexCount = uint(meshlets[meshletIndex].vertexCount);
	const uint triangleCount = uint(meshlets[meshletIndex].triangleCount);
	const uint indexCount = triangleCount * 3;

	const uint vertexOffset = meshlets[meshletIndex].vertexOffset;
	const uint indexOffset = vertexOffset + vertexCount;

#if DEBUG
	vec3 meshletColor = IntToColor(meshletIndex);
#endif

	// Vertices
	for (uint i = localThreadId; i < vertexCount; i += GROUP_SIZE)
	{
		uint vi = meshletData[vertexOffset + i] + meshDraw.vertexOffset;

		vec3 posOS = vec3(vertices[vi].px, vertices[vi].py, vertices[vi].pz);

		// position.z = position.z * 0.5 + 0.5;
		vec4 position = _View.viewProjMatrix * worldMat * vec4(posOS, 1.0);
		vec3 normal = vec3(uint(vertices[vi].nx), uint(vertices[vi].ny), uint(vertices[vi].nz)) / 127.0 - 1.0;
		vec2 texcoord = vec2(vertices[vi].s, vertices[vi].t);

		gl_MeshVerticesNV[i].gl_Position = position;
	#if !DEBUG
		OUT[i].outNormal = normal;
	#else
		OUT[i].outNormal = meshletColor;
	#endif
		OUT[i].outUV = texcoord;
	}

	// SetMeshOutputEXT();

	// Primitives
#if USE_PACKED_PRIMITIVE_INDICES_NV
	uint indexGroupCount = (indexCount + 3) / 4;
	for (uint i = localThreadId; i < indexGroupCount; i += GROUP_SIZE)
	{
		writePackedPrimitiveIndices4x8NV(i * 4, meshletData[indexOffset + i]);
	}

#else
	for (uint i = localThreadId; i < triangleCount; i += GROUP_SIZE)
	{
		uint indices = meshletData[indexOffset + i];
		gl_PrimitiveIndicesNV[i * 3 + 0] = indices & 0xFF;
		gl_PrimitiveIndicesNV[i * 3 + 1] = (indices >>  8) & 0xFF;
		gl_PrimitiveIndicesNV[i * 3 + 2] = (indices >> 16) & 0xFF;
	}	
#endif

#if USE_PER_PRIMITIVE
	for (uint i = localThreadId; i < triangleCount; i += GROUP_SIZE)
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
	
	if (localThreadId == 0)
		gl_PrimitiveCountNV = uint(triangleCount);
}
