#version 450

#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require

#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle_relative : require


#define DESC_VIEW_UNIFORMS 0
#include "Common.h"
#include "MetaballCommon.h"

#define MESH_GROUP_SIZE 64
#define MESH_GROUP_SIZE_X 8
#define MESH_GROUP_SIZE_Y 8

#define MAX_VERTICES 12
#define MAX_PRIMITIVES 5

#define USE_SUBGROUP 1
#define USE_WARP 1

const vec4 c_Sphere = vec4(0, 0, 0, 2);

taskPayloadSharedEXT TaskPayload payload;

layout (location = 0) out Interpolant
{
    vec3 outNormal;
    // vec2 outUV;
} OUT[];


struct Corner
{
    float value;
    vec3 n;
};

shared Corner sh_Fields[8];
shared uint sh_CubeIndex;

layout (max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;
layout (triangles) out;
#if USE_WARP
layout (local_size_x = 2, local_size_y = 2, local_size_z = 2*4) in;
#else
layout (local_size_x = 2, local_size_y = 2, local_size_z = 2 ) in;
#endif
void main()
{
    const uint localThreadIndex = gl_LocalInvocationIndex;
    const uvec3 workGroupId = gl_WorkGroupID;
    const uvec3 localThreadId = gl_LocalInvocationID;
    const uvec3 globalThreadId = gl_GlobalInvocationID;

#if 0
    const uint meshletIndex = workGroupId.x / TASK_MESHLET_SIZE;
    uvec3 baseMeshletId = payload.meshletIds[meshletIndex];
    uvec3 meshletId = baseMeshletId * TASK_MESHLET_SIZE + uvec3(workGroupId.x & (TASK_MESHLET_SIZE-1), workGroupId.y, workGroupId.z);
#else
    const uint meshletIndex = workGroupId.x;
    uvec3 meshletId = payload.meshletIds[meshletIndex];
#endif

    const vec3 bmin = _Constants.bmin.xyz;
    const vec3 bsize = _Constants.bsize.xyz;
    const uvec3 resolution = _Constants.resolution.xyz;

    uint nVert = 0, nTri = 0;

#if !USE_SUBGROUP
    if (localThreadIndex == 0)
        sh_CubeIndex = 0;
#endif

    // barrier(); // no need

    const vec3 step = bsize.xyz / vec3(resolution);
    const vec3 epsilon = (1.0f / 16.0f) * step;
    vec3 pmin = vec3(0.0), pmax = vec3(0.0);
    vec3 p = vec3(0.0);

    uint index = 0;

    // if (globalThreadId.x < resolution.x && globalThreadId.y < resolution.y && globalThreadId.z < resolution.z)
    {
        pmin = bmin.xyz + step * vec3(meshletId);
        pmax = pmin + step;
        p = pmin + step * vec3(localThreadId & 1);

    #if !USE_WARP
        float f = Field(p); 
        sh_Fields[localThreadIndex].value = f;

        vec3 n;
        // +X
        vec3 px = p + vec3(epsilon.x, 0, 0);
        n.x = Field(px) - f;
        // +Y
        vec3 py = p + vec3(0, epsilon.y, 0);
        n.y = Field(py) - f;
        // +Z
        vec3 pz = p + vec3(0, 0, epsilon.z);
        n.z = Field(pz) - f;

        n = SafeNormalize(n / epsilon);

        sh_Fields[localThreadIndex].n = (n);

    #else
        uint offset = localThreadId.z >> 1;
        p.x += (offset == 1) ? epsilon.x : 0;
        p.y += (offset == 2) ? epsilon.y : 0;
        p.z += (offset == 3) ? epsilon.z : 0;

        float f = Field(p); 

        vec3 n;
        // Grab data from other lanes so we can compute the normal by computing the difference in value in x, y and z directions
        n.x = subgroupShuffleDown(f,  8);
        n.y = subgroupShuffleDown(f, 16);
        n.z = subgroupShuffleDown(f, 24);
    
        if (localThreadIndex < 8)
        {
            n = SafeNormalize((n - f) / epsilon); // actually no need to divide, but to improve precision
            sh_Fields[localThreadIndex] = Corner(f, n);
        }
    #endif

    #if USE_SUBGROUP
        uvec4 ballot = subgroupBallot(f < 0);
        index = ballot.x & 0xFF;
    #else
        uint d = f >= 0.0 ? 0 : 1; // 0 - outside, 1 - inside
        atomicOr(sh_CubeIndex, (d << localThreadIndex));
    #endif
    }

    // barrier(); // no need

#if !USE_SUBGROUP
    index = sh_CubeIndex;
#endif

    nVert = uint(marchingCubesLookup[index].vertexCount);
    nTri = uint(marchingCubesLookup[index].triangleCount);

    SetMeshOutputsEXT(nVert, nTri);

    if (nVert == 0 || nTri == 0)
        return;

    const uint GroupSize = USE_WARP > 0 ? 32 : 8;
    for (uint vi = localThreadIndex; vi < nVert; vi += GroupSize)
    {
        uint edge = uint(marchingCubesLookup[index].vertices[vi]);
        uint i0 = edge & 7, i1 = (edge >> 3) & 7;
        vec3 p0 = vec3(
            (i0 & 1) > 0 ? pmax.x : pmin.x, 
            (i0 & 2) > 0 ? pmax.y : pmin.y, 
            (i0 & 4) > 0 ? pmax.z : pmin.z);
        vec3 p1 = vec3(
            (i1 & 1) > 0 ? pmax.x : pmin.x, 
            (i1 & 2) > 0 ? pmax.y : pmin.y, 
            (i1 & 4) > 0 ? pmax.z : pmin.z);

        float mixF = (sh_Fields[i0].value) / (sh_Fields[i0].value - sh_Fields[i1].value);
        vec3 posOS = mix(p0, p1, mixF);
        vec3 normalOS =  mix(sh_Fields[i0].n, sh_Fields[i1].n, mixF);
        // normalOS = MetaBallNormal(posOS); // SafeNormalize

        vec4 posCS = _View.viewProjMatrix * vec4(posOS, 1.0);
        gl_MeshVerticesEXT[vi].gl_Position = posCS;

        OUT[vi].outNormal = (normalOS);
    }

    if (localThreadIndex < nTri)
    {
        uint tri = marchingCubesLookup[index].triangles[localThreadIndex];
        gl_PrimitiveTriangleIndicesEXT[localThreadIndex] = uvec3(tri & 0xFF, (tri >> 8) & 0xFF, (tri >> 16) & 0xFF);
    }
}
