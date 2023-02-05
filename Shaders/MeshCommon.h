#ifndef MESH_COMMON_INCLUDED
#define MESH_COMMON_INCLUDED

#define USE_8BIT_16BIT_EXTENSIONS 1
// https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_shader_16bit_storage.txt


#if USE_8BIT_16BIT_EXTENSIONS
#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
#endif

#include "Common.h"

#ifndef USE_EXT_MESH_SHADER
#define USE_EXT_MESH_SHADER 1
#endif

#define MAX_VERTICES 64
#define MAX_PRIMITIVES 84
#define MAX_LODS 8

#define TASK_GROUP_SIZE 32
#define MESH_GROUP_SIZE 32

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

	// Used by traditional raster
	uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    // Used by mesh shading path
#if defined(USE_NV_MESH_SHADER)
    // NV
    uint taskCount;
    uint firstTask;

#elif defined(USE_EXT_MESH_SHADER)
    // EXT
    uint taskOffset;
    uint taskCount;
    uint groupCountX;
	uint groupCountY;
	uint groupCountZ;
#endif
};

struct TaskPayload
{
	uint drawId;
	uint meshletIndices[TASK_GROUP_SIZE];
};

// Deubg
#if 0
struct DebugParams
{
	// Draw culls
	uint drawFrustumCulling;
	uint drawOcclusionCulling;
	// Meshlet culls
	uint meshletConeCulling;
	uint meshletFrustumCulling;
	uint meshletOcclusionCulling;
	// Triangel culls
	uint triBackfaceCulling;
	uint triSmallCulling;
};

#else

#define DESC_DEBUG_UNIFORMS (DESC_VIEW_UNIFORMS+1)

layout (binding = DESC_DEBUG_UNIFORMS) uniform DebugParams
{
	// Draw culls
	uint drawFrustumCulling;
	uint drawOcclusionCulling;
	// Meshlet culls
	uint meshletConeCulling;
	uint meshletFrustumCulling;
	uint meshletOcclusionCulling;
	// Triangel culls
	uint triBackfaceCulling;
	uint triSmallCulling;

	uint toyDraw;
} _DebugParams;
#endif

#endif // MESH_COMMON_INCLUDED
