#version 450 core

#define VERTEX_INPUT_MODE 1
#define VERTEX_ALIGNMENT 1
#define USE_8BIT_16BIT_EXTENSIONS 1
// https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_shader_16bit_storage.txt

#if USE_8BIT_16BIT_EXTENSIONS
#extension GL_EXT_shader_16bit_storage  : require
#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require
// #extension GL_KHR_shader_explicit_arithmetic_types  : require
#endif

#if VERTEX_INPUT_MODE == 1
struct Vertex
{
#if !VERTEX_ALIGNMENT

    vec3 position;
#if USE_8BIT_16BIT_EXTENSIONS
    // f16vec3 position;
    u8vec4 normal;
    f16vec2 texcoord0;
#else
    vec3 normal;
    vec2 texcoord0;
#endif

#else

    float px, py, pz;
#if USE_8BIT_16BIT_EXTENSIONS
    // float16_t px, py, pz;
    uint8_t nx, ny, nz, nw;
    float16_t s, t;
#else
    float nx, ny, nz;
    float s, t;
#endif

#endif
};

layout (std430, binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
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
    vec3 position = vec3(vertices[gl_VertexIndex].px, vertices[gl_VertexIndex].py, vertices[gl_VertexIndex].pz);
#if USE_8BIT_16BIT_EXTENSIONS
    vec3 fNormal = vec3(int(vertices[gl_VertexIndex].nx), int(vertices[gl_VertexIndex].ny), int(vertices[gl_VertexIndex].nz)) / 127.0f - 1.0f;
#else // USE_8BIT_16BIT_EXTENSIONS
    vec3 fNormal = vec3(vertices[gl_VertexIndex].nx, vertices[gl_VertexIndex].ny, vertices[gl_VertexIndex].nz);
#endif // USE_8BIT_16BIT_EXTENSIONS
    vec2 texcoord0 = vec2(vertices[gl_VertexIndex].s, vertices[gl_VertexIndex].t);
#endif // VERTEX_ALIGNMENT

#else // VERTEX_INPUT_MODE

#if USE_8BIT_16BIT_EXTENSIONS
    vec3 fNormal = vec3(normal.xyz) / 127.0f - 1.0f;
#endif // USE_8BIT_16BIT_EXTENSIONS

#endif // VERTEX_INPUT_MODE

    gl_Position = vec4(position, 1.0f);

    outNormal = fNormal;
    outUV = texcoord0;
}
