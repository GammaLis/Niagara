#version 450 core

#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
#extension GL_NV_mesh_shader	: require
// #define GL_EXT_mesh_shader 1

struct Vertex
{
	float px, py, pz;
	uint8_t nx, ny, nz, nw;
	float s, t;
};

struct Meshlet
{
	uint vertices[64];
	uint8_t indices[126]; // up to 42 triangels
	uint8_t vertexCount;
	uint8_t indexCount;
};

layout (std430, binding = 0) buffer Vertices
{
	Vertex vertices[];
};

layout (std430, binding = 1) buffer Meshlets
{
	Meshlet meshlets[];
};

// Set the number of threads per workgroup (always one-dimensional)
layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
// Mesh shader
// The primitive type (points, lines, triangles)
layout (triangles) out;
// Maximum allocation size for each meshlet
layout (max_vertices = 64, max_primitives = 42) out;

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

#endif

/**
 * gl_NumWorkGroups -> NumWorkgroups decorated OpVariable (existing)
 * gl_WorkGroupSize -> WorkgroupSize decorated OpVariable (existing)
 * gl_WorkGroupID -> WorkgroupId decorated OpVariable (existing)
 * gl_LocalInvocationID -> LocalInvocationId decorated OpVariable (existing)
 * gl_GlobalInvocationID -> GlobalInvocationId decorated OpVariable (existing)
 * gl_LocalInvocationIndex -> LocalInvocationIndex decorated OpVariable (existing)
 * gl_PrimitivePointIndicesEXT -> PrimitivePointIndicesEXT decorated OpVariable
 * gl_PrimitiveLineIndicesEXT -> PrimitiveLineIndicesEXT decorated OpVariable
 * gl_PrimitiveTriangleIndicesEXT -> PrimitiveTriangleIndicesEXT decorated OpVariable
 * gl_Position -> Position decorated OpVariable (existing)
 * gl_PointSize -> PointSize decorated OpVariable (existing)
 * gl_ClipDistance -> ClipDistance decorated OpVariable (existing)
 * gl_CullDistance -> CullDistance decorated OpVariable (existing)
 * gl_PrimitiveID -> PrimitiveId decorated OpVariable (existing)
 * gl_Layer -> Layer decorated OpVariable (existing)
 * gl_ViewportIndex -> ViewportIndex decorated OpVariable (existing)
 * gl_CullPrimitiveEXT -> CullPrimitiveEXT decorated OpVariable
 * gl_DrawID -> DrawIndex decorated OpVariable (existing 1.3, extension)
 *
 * EmitMeshTasksEXT -> OpEmitMeshTasksEXT()
 * SetMeshOutputsEXT -> OpSetMeshOutputsEXT()
 */

void main()
{
	uint meshletIndex = gl_WorkGroupID.x;

	// Vertices
	for (uint i = 0; i < uint(meshlets[meshletIndex].vertexCount); ++i)
	{
		uint vi = meshlets[meshletIndex].vertices[i];

		vec3 position = vec3(vertices[vi].px, vertices[vi].py, vertices[vi].pz);
		vec3 normal = vec3(uint(vertices[vi].nx), uint(vertices[vi].ny), uint(vertices[vi].nz)) / 127.0 - 1.0;
		vec2 texcoord = vec2(vertices[vi].s, vertices[vi].t);

		gl_MeshVerticesNV[i].gl_Position = vec4(position, 1.0);
		OUT[i].outNormal = normal;
		OUT[i].outUV = texcoord;
	}

	// SetMeshOutputEXT();

	// Primitives
	gl_PrimitiveCountNV = uint(meshlets[meshletIndex].indexCount) / 3;

	for (uint i = 0; i < meshlets[meshletIndex].indexCount; ++i)
	{
		gl_PrimitiveIndicesNV[i] = meshlets[meshletIndex].indices[i];
	}
}
