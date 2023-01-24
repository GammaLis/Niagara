#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED

layout (set = 0, binding = 4) uniform ViewUniformBufferParameters
{
    mat4 viewProjMatrix;
    vec4 debugValue;
} _View;

#endif // COMMON_INCLUDED
