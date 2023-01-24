#version 450

#extension GL_GOOGLE_include_directive  : require
#include "Common.h"

#define USE_PER_PRIMITIVE 0

#if USE_PER_PRIMITIVE
#extension GL_NV_mesh_shader : require
#endif

layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 uv;

#if USE_PER_PRIMITIVE
layout (location = 2) in perprimitiveNV vec3 triangleNormal;
#endif

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(normal * 0.5 + 0.5, 1.0) * _View.debugValue;
}
