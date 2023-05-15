#version 450

layout(binding = 0) uniform sampler2D _MainTex;

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 col = texture(_MainTex, uv).rgb;

    outColor = vec4(col, 1.0);
}
