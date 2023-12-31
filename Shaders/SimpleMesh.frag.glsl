#version 450

#define USE_PER_PRIMITIVE 0

#extension GL_GOOGLE_include_directive  : require
#include "Common.h"

// >> Debug
#define OVERDRAW_COLOR vec4(0.1, 0.1, 0.1, 1.0)
#define DEBUG_MODE_NORMAL 1
#define DEBUG_MODE_UV 2
#define DEBUG_MODE_OVERDARW 3

#define DEBUG_MODE DEBUG_MODE_NORMAL


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
#if DEBUG_MODE == DEBUG_MODE_NORMAL

#if 1
    vec3 N = SafeNormalize(normal);
    outColor = vec4(N * 0.5 + 0.5, 1.0) * _View.debugValue;

#else
    // Debug meshlet
    outColor = vec4(normal, 1.0);
#endif

#elif DEBUG_MODE == DEBUG_MODE_UV
    outColor = vec4(uv, 0, 1);
    
#elif DEBUG_MODE == DEBUG_MODE_OVERDARW
    outColor = OVERDRAW_COLOR;

#else
    // TODO: 
    outColor = (0, 0, 0, 1);
#endif
}
