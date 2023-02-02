// Ref: https://rg3.name/202210222107.html
// My mesh shaders talk at XDC 2022

#version 450
#extension GL_EXT_mesh_shader : enable

#define MAX_VERTICES 64
#define MAX_PRIMITIVES 84

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in; // Typical limit: 128 invocations
layout (triangles) out;
layout (max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;

layout (location = 0) out vec4 out0[]; // per-vertex
layout (location = 1) out perprimitiveEXT out vec4 out1[]; // per-primitive

void main()
{
	// Typical compute built-ins: gl_NumWorkGroups, gl_WorkGroupID, gl_LocalInvocationID, ...
	// Typical subgroup functionality: gl_NumSubgropus, gl_SubgroupID, subgroupElect(), ...
	
	SetMeshOutputsEXT(ACTUAL_V, ACTUAL_P);

	gl_MeshVerticesEXT[FOO].gl_Position = vec4(...);
	gl_PrimitiveTriangleIndicesEXT[BAR] = uvec3(...);
}

// Some built-in arrays
// Write only access
out uint gl_PrimitivePointIndicesEXT[];
out uvec2 gl_PrimitiveLineIndicesEXT[];
out uvec3 gl_PrimitiveTriangleIndicesEXT[];

// Write only access
out gl_MeshPerVertexEXT {
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
	float gl_CullDistance[];
} gl_MeshVerticesEXT[];

// Write only access
perprimitiveEXT out gl_MeshPerPrimitiveEXT {
	int gl_PrimitiveID;
	int gl_Layer;
	int gl_ViewportIndex;
	bool gl_CullPrimitiveEXT;
	int gl_PrimitiveShadingRateEXT;
} gl_MeshPrimitivesEXT[];
