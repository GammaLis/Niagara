#ifndef METABALL_COMMON_INCLUDED
#define METABALL_COMMON_INCLUDED

#define TASK_MESHLET_SIZE 4

struct Lookup
{
    uint triangles[5];
    uint8_t vertices[12];
    uint8_t triangleCount;
    uint8_t vertexCount;
};

struct TaskPayload
{
	uvec3 meshletIds[64];
};

// Bounding box
layout (push_constant) uniform PushConstants
{
    vec4 bmin; // xyz - bmin
    vec4 bsize;// xyz - bsize
    uvec4 resolution; // xyz - resolution
} _Constants;

layout (std430, binding = 1) readonly buffer MarchingCubesLookup
{
    Lookup marchingCubesLookup[];
};

layout (std430, binding = 2) readonly buffer Metaballs
{
    vec4 balls[];
};

vec4 GetSphere()
{
#if 0
    const vec3 c = _Constants.bmin.xyz + 0.5 * _Constants.bsize.xyz;
    const float r = min(_Constants.bsize.x, min(_Constants.bsize.y, _Constants.bsize.z)) * 0.5f;
    const vec4 sphere = vec4(c, r);
#else
    const vec4 sphere = balls[0];
#endif

    return sphere;
}

// Ref: https://www.shadertoy.com/view/ld2GRz
float Field(vec3 pos)
{
#if 1

    vec4 sphere = GetSphere();

#if 1
    vec3 d = pos - sphere.xyz;
    return sqrt(dot(d, d)) - sphere.w;

#else
    // (x^2 + 9/4*y^2 + z^2-1)^3 - x^2 * z^3 - 9/200 * y^2 * z^3 = 0
    vec3 p = pos - sphere.xyz;
    p.xyz = p.xzy;
    float x2 = p.x * p.x, y2 = p.y * p.y, z2 = p.z * p.z;
    float z3 = z2 * p.z;
    float f = x2 + 9./4. * y2 + z2 - 1.0;
    f = f * f * f - x2 * z3 - 9./200.* y2 * z3;
    return f;
#endif

#elif 1
    const uint Count = _Constants.resolution.w;

    // Metalls
    float sum = -1.0f;
    float m = 0.0;
    float p = 0.0;
    float dmin = 1e4f;

    float h = 1.0; // trace Lipschitz costant

    for (uint i = 0; i < Count; ++i)
    {
        // vec3 d = balls[i].xyz - pos;
        // sum += (balls[i].w * balls[i].w) / dot(d, d);

        float db = length(balls[i].xyz - pos);
        if (db < balls[i].w)
        {
            float x = db / balls[i].w;
            p += 1.0 - x*x*x*(x*(x*6.0-15.0)+10.0);
            m += 1.0;
            h = max(h, 0.5333*balls[i].w);
        }
        else 
        {
            dmin = min(dmin, db - balls[i].w);
        }
    }

    float d = dmin + 0.1;
    if (m > 0.5)
    {
        float th = 0.2;
        d = h * (th - p);
    }

    sum = d;

    // Ref: Humus - Metaballs
    // Occasionally some balls swing across the outer edge of the volume, causing a visible cut.
    // For more visually pleasing results, we manipulate the field a bit with a fade gradient near the edge, so that the balls flatten against the edge instead.
#if 0
    vec3 d = abs(p - vec3(0.5f));
    float edge_dist = max(d.x, max(d.y, d.z));
    sum = sum * min(0.5f * (0.5f - edge_dist), 1.0f) - 0.001f;
#endif

    return sum;

#endif
}

// Ref: Metaballs - Quintic 
// https://www.shadertoy.com/view/ld2GRz 
vec3 MetaBallNormal(vec3 pos)
{
    vec3 nor = vec3( 0.0, 0.0001, 0.0 );

    const uint Count = _Constants.resolution.w;

    // Metalls
    float sum = -1.0f;
    float m = 0.0;
    float p = 0.0;
    float dmin = 1e4f;

    float h = 1.0; // trace Lipschitz costant

    for(uint i = 0; i < Count; i++)
    {
        float db = length(balls[i].xyz - pos);
        float x = clamp( db/balls[i].w, 0.0, 1.0 );
        float p = x*x*(30.0*x*x - 60.0*x + 30.0);
        nor += normalize(pos - balls[i].xyz) * p / balls[i].w;
    }
    
    return normalize(nor);
}

#endif // METABALL_COMMON_INCLUDED
