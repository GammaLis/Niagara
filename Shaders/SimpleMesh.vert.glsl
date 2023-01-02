#version 450

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texcoord0;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main()
{
    gl_Position = vec4(vertex, 1.0f);
    outNormal = normal;
    outUV = texcoord0;
}
