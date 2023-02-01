#ifndef MESH_COMMON_INCLUDED
#define MESH_COMMON_INCLUDED

#define USE_8BIT_16BIT_EXTENSIONS 1


#if USE_8BIT_16BIT_EXTENSIONS
#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
#endif

#include "Common.h"


#define MAX_VERTICES 64
#define MAX_PRIMITIVES 84
#define MAX_LODS 8

#define DESC_VERTEX_BUFFER 0
#define DESC_MESH_BUFFER 1
#define DESC_DRAW_DATA_BUFFER 2
#define DESC_DRAW_COMMAND_BUFFER 3
#define DESC_MESHLET_BUFFER 4
#define DESC_MESHLET_DATA_BUFFER 5


struct Vertex
{
	float px, py, pz;
#if USE_8BIT_16BIT_EXTENSIONS
	// float16_t px, py, pz;
	uint8_t nx, ny, nz, nw;
	float16_t s, t;
#else
	float nx, ny, nz;
	float s, t;
#endif
	
};

struct Meshlet
{
	vec4 boundingSphere; // xyz - center, w - radius
	vec4 coneApex;
	vec4 cone;
	uint vertexOffset;
	uint8_t vertexCount;
	uint8_t triangleCount;
};

struct MeshLod
{
	uint indexOffset;
	uint indexCount;
	uint meshletOffset;
	uint meshletCount;
};

struct Mesh
{
	vec4 boundingSphere;
	int  vertexOffset;
	uint vertexCount;
	uint lodCount;
	MeshLod lods[MAX_LODS];
};

struct MeshDraw
{
	vec4 worldMatRow0;
	vec4 worldMatRow1;
	vec4 worldMatRow2;

	int  vertexOffset;	
	uint meshIndex;
};

struct MeshDrawCommand
{
	uint drawId;

	uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    uint taskCount;
    uint firstTask;
};


/// Functions

mat4 BuildWorldMatrix(vec4 row0, vec4 row1, vec4 row2)
{
	return mat4(
		vec4(row0.x, row1.x, row2.x, 0.0f),
        vec4(row0.y, row1.y, row2.y, 0.0f), 
        vec4(row0.z, row1.z, row2.z, 0.0f),
        vec4(row0.w, row1.w, row2.w, 1.0f));
}

vec3 GetScaleFromWorldMatrix(mat4 worldMatrix)
{
	float sx = sqrt(dot(worldMatrix[0], worldMatrix[0]));
	float sy = sqrt(dot(worldMatrix[1], worldMatrix[1]));
	float sz = sqrt(dot(worldMatrix[2], worldMatrix[2]));
	return vec3(sx, sy, sz);
}

// Ref: UE - RayTracingDebug.usf
uint MurmurMix(uint Hash)
{
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}

vec3 IntToColor(uint Index)
{
	uint Hash  = MurmurMix(Index); 

	vec3 Color = vec3
	(
		(Hash >>  0) & 255,
		(Hash >>  8) & 255,
		(Hash >> 16) & 255
	);

	return Color * (1.0f / 255.0f);
}

#endif // MESH_COMMON_INCLUDED
