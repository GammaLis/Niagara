#version 450 core

#extension GL_GOOGLE_include_directive  : require
#include "MeshCommon.h"

#extension GL_ARB_shader_draw_parameters    : require // gl_DrawIDARB


#define VERTEX_INPUT_MODE 1
#define VERTEX_ALIGNMENT 1


#if 0
layout (binding = 0) uniform ObjectUniformBuffer
{
    mat4 objToWorldMatrix;
};
#endif


#if !VERTEX_ALIGNMENT
struct Vertex
{

    vec3 position;
#if USE_8BIT_16BIT_EXTENSIONS
    // f16vec3 position;
    u8vec4 normal;
    f16vec2 texcoord0;
#else
    vec3 normal;
    vec2 texcoord0;
#endif

};
#endif

#if VERTEX_INPUT_MODE == 1


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

#else // VERTEX_INPUT_MODE

#if USE_8BIT_16BIT_EXTENSIONS
layout (location = 0) in vec3 position;
layout (location = 1) in uvec4 normal;
layout (location = 2) in vec2 texcoord0;

#else
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texcoord0;

#endif // USE_8BIT_16BIT_EXTENSIONS

#endif // VERTEX_INPUT_MODE

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main()
{
#if VERTEX_INPUT_MODE == 1
    // The SPIR-V Capability (Int8) was declared, but none of the requirements were met to use it.
    // => Access the buffer directly.
    // Vertex v = vertices[gl_VertexIndex];

    MeshDrawCommand meshDrawCommand = drawCommands[gl_DrawIDARB];
    MeshDraw meshDraw = draws[meshDrawCommand.drawId];

#if !VERTEX_ALIGNMENT
    // NOT USED NOW
    vec3 position = v.position;
#if USE_8BIT_16BIT_EXTENSIONS
    vec3 normal = v.normal / 127.0f - 1.0f;
#else
    vec3 normal = v.normal;
#endif
    vec2 texcoord0 = v.texcoord0;

#else // VERTEX_ALIGNMENT
    vec3 posOS = vec3(vertices[gl_VertexIndex].px, vertices[gl_VertexIndex].py, vertices[gl_VertexIndex].pz);
#if USE_8BIT_16BIT_EXTENSIONS
    vec3 normalOS = vec3(int(vertices[gl_VertexIndex].nx), int(vertices[gl_VertexIndex].ny), int(vertices[gl_VertexIndex].nz)) / 127.0f - 1.0f;
#else // USE_8BIT_16BIT_EXTENSIONS
    vec3 normalOS = vec3(vertices[gl_VertexIndex].nx, vertices[gl_VertexIndex].ny, vertices[gl_VertexIndex].nz);
#endif // USE_8BIT_16BIT_EXTENSIONS
    vec2 texcoord0 = vec2(vertices[gl_VertexIndex].s, vertices[gl_VertexIndex].t);
#endif // VERTEX_ALIGNMENT

#else // VERTEX_INPUT_MODE

#if USE_8BIT_16BIT_EXTENSIONS
    vec3 normalOS = vec3(normal.xyz) / 127.0f - 1.0f;
#endif // USE_8BIT_16BIT_EXTENSIONS

#endif // VERTEX_INPUT_MODE

    const mat4 worldMat = mat4(
        vec4(meshDraw.worldMatRow0.x, meshDraw.worldMatRow1.x, meshDraw.worldMatRow2.x, 0.0f),
        vec4(meshDraw.worldMatRow0.y, meshDraw.worldMatRow1.y, meshDraw.worldMatRow2.y, 0.0f), 
        vec4(meshDraw.worldMatRow0.z, meshDraw.worldMatRow1.z, meshDraw.worldMatRow2.z, 0.0f),
        vec4(meshDraw.worldMatRow0.w, meshDraw.worldMatRow1.w, meshDraw.worldMatRow2.w, 1.0f));
    vec4 position = vec4(posOS, 1.0);
    position = _View.viewProjMatrix * worldMat * position;

    gl_Position = position;

    outNormal = normalOS;
    outUV = texcoord0;
}
