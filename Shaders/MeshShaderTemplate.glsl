// Ref: My mesh shaders talk at XDC 2022, https://rg3.name/202210222107.html
// Ref: https://www.khronos.org/blog/mesh-shading-for-vulkan

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
out uint  gl_PrimitivePointIndicesEXT[];
out uvec2 gl_PrimitiveLineIndicesEXT[];
out uvec3 gl_PrimitiveTriangleIndicesEXT[];

// Write only access
out gl_MeshPerVertexEXT {
	vec4  gl_Position;
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


// Task shaders
/**
 * Task shader dispatching mesh shader workgroups
 * Single workgroup-uniform call to EmitMeshTasksEXT(x, y, z);
 *
 * Ref: https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_mesh_shader.adoc
 * EmitMeshTasksEXT, which takes as input a number of mesh shader groups to emit, and a payload variable that will be visible to 
 * all mesh shader invocations launched by this instruction. **This instruction is executed once per workgroup rather than per-invocation**, 
 * and the payload itself is in a workgroup-wide storage class, similar to shared memory. 
 * **Once this instruction is called, the workgroup is terminated immediately, and the mesh shaders are launched**.
 */
