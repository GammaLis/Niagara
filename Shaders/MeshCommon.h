#ifndef MESH_COMMON_INCLUDED
#define MESH_COMMON_INCLUDED

#define USE_8BIT_16BIT_EXTENSIONS 1
#define USE_PER_PRIMITIVE 0

#if USE_8BIT_16BIT_EXTENSIONS
#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
#endif


#define MAX_VERTICES 64
#define MAX_PRIMITIVES 84


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
	vec4 cone;
	uint32_t vertexOffset;
	uint8_t vertexCount;
	uint8_t triangleCount;
};


/// Functions
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
