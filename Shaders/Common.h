#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED

#define DESC_VIEW_UNIFORMS 6

layout (set = 0, binding = 6) uniform ViewUniformBufferParameters
{
    mat4 viewProjMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    vec4 frustumPlanes[6];
    vec4 frustumValues; // X L/R plane -> (+/-X, 0, Z, 0), Y U/D plane -> (0, +/-Y, Z, 0)
    vec4 zNearFar; // x - near, y - far, zw - not used
    vec4 viewportRect;
    vec4 depthPyramidSize; // xy - viewport size, zw - texture size
    vec4 debugValue;
    vec3 camPos;
    uint drawCount;
} _View;

#ifndef EPS 
#define EPS 1e-5
#endif

#define FLOAT_MAX 3.402823466e+38

#ifndef PI
const float PI = 3.1415926535897932f;
#endif

/// Functions

mat4 BuildWorldMatrix(vec4 row0, vec4 row1, vec4 row2)
{
    return mat4(
        vec4(row0.x, row1.x, row2.x, 0.0f),
        vec4(row0.y, row1.y, row2.y, 0.0f), 
        vec4(row0.z, row1.z, row2.z, 0.0f),
        vec4(row0.w, row1.w, row2.w, 1.0f));
}

vec3 GetScaleFromWorldMatrix(mat4 worldMatrix)
{
    float sx = sqrt(dot(worldMatrix[0], worldMatrix[0]));
    float sy = sqrt(dot(worldMatrix[1], worldMatrix[1]));
    float sz = sqrt(dot(worldMatrix[2], worldMatrix[2]));
    return vec3(sx, sy, sz);
}

float ConvertToDeviceZ(float sceneDepth)
{
    // Perspective
    return -_View.projMatrix[2][2] - _View.projMatrix[3][2] / sceneDepth; // or abs(...)
}


// Ref: UE - RayTracingDebug.usf
uint MurmurMix(uint Hash)
{
    Hash ^= Hash >> 16;
    Hash *= 0x85ebca6b;
    Hash ^= Hash >> 13;
    Hash *= 0xc2b2ae35;
    Hash ^= Hash >> 16;
    return Hash;
}

vec3 IntToColor(uint Index)
{
    uint Hash  = MurmurMix(Index); 

    vec3 Color = vec3
    (
        (Hash >>  0) & 255,
        (Hash >>  8) & 255,
        (Hash >> 16) & 255
    );

    return Color * (1.0f / 255.0f);
}

// Cull

// View space frustum calling
bool FrustumCull(vec4 boundingSphere)
{
    bool culled = false;
    // L/R/T/B
#if 0
    for (uint i = 0; i < 4; ++i)
    {
        culled = dot(_View.frustumPlanes[i], vec4(boundingSphere.xyz, 1.0)) > boundingSphere.w;
        if (culled)
            return true;
    }
#else
    culled =           -abs(boundingSphere.x) * _View.frustumValues.x + boundingSphere.z * _View.frustumValues.y > boundingSphere.w;
    culled = culled || -abs(boundingSphere.y) * _View.frustumValues.z + boundingSphere.z * _View.frustumValues.w > boundingSphere.w;
#endif
    // Near/Far
    culled = culled || (boundingSphere.z - boundingSphere.w > -_View.zNearFar.x) || (boundingSphere.z + boundingSphere.w < -_View.zNearFar.y);

    return culled;
}

/**
 * >> MeshOptimizer
 * For backface culling with orthographic projection, use the following formula to reject backfacing clusters:
 *   dot(view, cone_axis) >= cone_cutoff
 *
 * For perspective projection, you can the formula that needs cone apex in addition to axis & cutoff:
 *   dot(normalize(cone_apex - camera_position), cone_axis) >= cone_cutoff
 *
 * Alternatively, you can use the formula that doesn't need cone apex and uses bounding sphere instead:
 *   dot(normalize(center - camera_position), cone_axis) >= cone_cutoff + radius / length(center - camera_position)
 * or an equivalent formula that doesn't have a singularity at center = camera_position:
 *   dot(center - camera_position, cone_axis) >= cone_cutoff * length(center - camera_position) + radius
 *
 * The formula that uses the apex is slightly more accurate but needs the apex; if you are already using bounding sphere
 * to do frustum/occlusion culling, the formula that doesn't use the apex may be preferable (for derivation see
 * Real-Time Rendering 4th Edition, section 19.3).
 */

bool ConeCull(vec4 cone, vec3 viewDir)
{
    float VdotCone = dot(viewDir, cone.xyz);
    float threshold = sqrt(1.0 - cone.w * cone.w);
    // FIXME:
    // return cone.w < 0 ? VdotCone > threshold : VdotCone < -threshold;
    // return cone.w > 0 && VdotCone < -threshold;
    return VdotCone >= cone.w;
}

bool ConeCull_ConeApex(vec4 cone, vec3 coneApex, vec3 camPos)
{
    return dot(normalize(coneApex - camPos), cone.xyz) >= cone.w;
}

bool ConeCull_BoundingSphere(vec4 cone, vec4 boundingSphere, vec3 camPos)
{
    vec3 sphereToCamVec = boundingSphere.xyz - camPos;
    return dot(cone.xyz, sphereToCamVec) >= cone.w * length(sphereToCamVec) + boundingSphere.w;
}

// Ref: 2D Polyhedral Bounds of a Clipeed Perspective-Projected 3D Sphere, jcgt 2013
bool GetAxisAlignedBoundingBox(
    vec4 sphere, // camera-space sphere center
    float nearZ, // near clipping plane position (negative)
    mat4 P,
    inout vec4 aabb)
{
    if (sphere.z + sphere.w > nearZ)
        return false;

    const float p00 = P[0][0], p11 = P[1][1];

#if 0
    vec3 C = vec3(sphere.x, sphere.y, sphere.z);
    float r = sphere.w;

    vec2 c = vec2(C.x, C.z);
    float c2 = dot(c, c);
    float t = sqrt(c2 - r * r);
    vec2 v = vec2(t, r) / sqrt(c2);

    vec2 minx = mat2(v.x, -v.y, +v.y, v.x) * c /* * v.x */;
    vec2 maxx = mat2(v.x, +v.y, -v.y, v.x) * c /* * v.x */;
    
    c = vec2(C.y, C.z);
    c2 = dot(c, c);
    t = sqrt(c2 - r * r);
    v = vec2(t, r) / sqrt(c2);
    vec2 miny = mat2(v.x, -v.y, +v.y, v.x) * c /* * v.x */;
    vec2 maxy = mat2(v.x, +v.y, -v.y, v.x) * c /* * v.x */;

    aabb = vec4(
        minx.x * p00 / abs(minx.y),
        miny.x * p11 / abs(miny.y), 
        maxx.x * p00 / abs(maxx.y),
        maxy.x * p11 / abs(maxy.y));
    aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f

#else
    // ==>
    const vec3  c = sphere.xyz;
    const float r = sphere.w;

    const float z2_r2 = c.z * c.z - r * r;
    const vec3  cr = c * r;

    float vx = sqrt(c.x * c.x + z2_r2);
    float minx = (vx * c.x + cr.z) / abs(-cr.x + vx * c.z);
    float maxx = (vx * c.x - cr.z) / abs(+cr.x + vx * c.z);

    float vy = sqrt(c.y * c.y + z2_r2);
    float miny = (vy * c.y + cr.z) / abs(-cr.y + vy * c.z);
    float maxy = (vy * c.y - cr.z) / abs(+cr.y + vy * c.z);

    aabb = vec4(minx * p00, miny * p11, maxx * p00, maxy * p11);
    aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f;
#endif  

    return true;
}

bool InsideBounds(vec2 p, vec4 bounds)
{
    return (p.x >= bounds.x && p.x < bounds.z) && 
           (p.y >= bounds.y && p.y < bounds.w);
}

#endif // COMMON_INCLUDED
