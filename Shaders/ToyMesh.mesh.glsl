#version 450

#extension GL_EXT_mesh_shader : require

#define MAX_VERTICES 3
#define MAX_PRIMITIVES 1
#define MESH_GROUP_SIZE 32

vec3 Colors[] = 
{
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),
};

vec2 Vertices[] = 
{
	vec2(-0.5f, -0.5f), 
	vec2( 0.0f, +0.5f),
	vec2(+0.5f, -00.5f)
};


layout (local_size_x = MESH_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
layout (triangles) out;
layout (max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;

layout (location = 0) out Interpolant
{
	vec2 uv;
} varyings[];

void main()
{
	const uint localThreadId = gl_LocalInvocationID.x;
	const uint globalThreadId = gl_GlobalInvocationID.x;

	uint vertexCount = 3;
	uint triangleCount = 1;

	SetMeshOutputsEXT(vertexCount, triangleCount);

	// Vertices
	if (globalThreadId < vertexCount)
	{
		const uint vertexIndex = globalThreadId;

		vec2 uv = vec2((vertexIndex & 1) << 1, vertexIndex & 2);

	#if 1
		gl_MeshVerticesEXT[vertexIndex].gl_Position = vec4(uv * vec2(+2.0, -2.0) + vec2(-1.0, +1.0), 0.0, 1.0);
	#else
		gl_MeshVerticesEXT[vertexIndex].gl_Position = vec4(Vertices[vertexIndex], 0.0, 1.0);
	#endif

		varyings[vertexIndex].uv = uv;
	}

	// Triangles
	if (globalThreadId < triangleCount)
	{
		const uint triangleIndex = globalThreadId;

		gl_PrimitiveTriangleIndicesEXT[triangleIndex] = uvec3(0, 1, 2);
	}
}
