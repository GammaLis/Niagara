#version 450

const vec2 g_Positions[3] = vec2[](
    vec2( 0.0, +0.577),
    vec2(-0.5, -0.288),
    vec2(+0.5, -0.288)
);

const vec3 g_Colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

const vec2 g_UVs[3] = vec2[](
    vec2(0.5, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;

void main()
{
    gl_Position = vec4(g_Positions[gl_VertexIndex], 0.0, 1.0);
    outColor    = g_Colors[gl_VertexIndex];
    outUV       = g_UVs[gl_VertexIndex];
}
