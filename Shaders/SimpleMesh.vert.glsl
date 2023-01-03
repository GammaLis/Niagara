#version 450 core

#define VERTEX_INPUT_MODE 1
#define VERTEX_ALIGNMENT 1

#if VERTEX_INPUT_MODE == 1
struct Vertex
{
#if !VERTEX_ALIGNMENT
    vec3 position;
    vec3 normal;
    vec2 texcoord0;
#else
    float px, py, pz;
    float nx, ny, nz;
    float s, t;
#endif
};

layout (std430, binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

#else
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texcoord0;

#endif

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main()
{
#if VERTEX_INPUT_MODE == 1
    Vertex v = vertices[gl_VertexIndex];

#if !VERTEX_ALIGNMENT
    vec3 position = v.position;
    vec3 normal = v.normal;
    vec2 texcoord0 = v.texcoord0;
#else
    vec3 position = vec3(v.px, v.py, v.pz);
    vec3 normal = vec3(v.nx, v.ny, v.nz);
    vec2 texcoord0 = vec2(v.s, v.t);
#endif
#endif

    gl_Position = vec4(position, 1.0f);

    outNormal = normal;
    outUV = texcoord0;
}
