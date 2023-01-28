#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED

layout (set = 0, binding = 6) uniform ViewUniformBufferParameters
{
    mat4 viewProjMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    vec4 frustumPlanes[6];
    vec4 debugValue;
    vec3 camPos;
} _View;

#ifndef EPS 
#define EPS 1e-5
#endif

#define FLOAT_MAX 3.402823466e+38

#ifndef PI
const float PI = 3.1415926535897932f;
#endif

#endif // COMMON_INCLUDED
