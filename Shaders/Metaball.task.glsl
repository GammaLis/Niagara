#version 450

#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_8bit_storage   : require
#extension GL_EXT_shader_explicit_arithmetic_types  : require

#extension GL_KHR_shader_subgroup_ballot : require

#include "MetaballCommon.h"

#define TASK_GROUP_SIZE 4

#define USE_SUBGROUP 1

taskPayloadSharedEXT TaskPayload payload;

shared uint sh_Count;

#if 0

layout (local_size_x = TASK_GROUP_SIZE, local_size_y = TASK_GROUP_SIZE, local_size_z = TASK_GROUP_SIZE) in;
void main()
{
    const uint localThreadIndex = gl_LocalInvocationIndex;
    const uvec3 workGroupId = gl_WorkGroupID;
    const uvec3 localThreadId = gl_LocalInvocationID;
    const uvec3 globalThreadId = gl_GlobalInvocationID;

    const vec3 bmin = _Constants.bmin.xyz;
    const vec3 bsize = _Constants.bsize.xyz;
    const uvec3 resolution = _Constants.resolution.xyz;

    const uvec3 taskResolution = resolution / TASK_MESHLET_SIZE;
    const uint meshletCountPerThread = taskResolution.x * taskResolution.y * taskResolution.z;

    vec3 step = bsize.xyz / vec3(taskResolution);
    vec3 pmin = bmin + step * vec3(globalThreadId);
    vec3 pmax = pmin + step;

    if (localThreadIndex == 0)
        sh_Count = 0;
    
    barrier();

    vec3 p = pmin;
    uint index = 0;
    for (uint i = 0; i < 8; ++i)
    {
        p = pmin + vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1) * step;
        float f = Field(p);
        index = index | ((f > 0 ? 0 : 1) << i);
    }

    bool intersect = index > 0 && index < 0xFF;

    uint id = 0;
    uint count = 0;

#if USE_SUBGROUP
    uvec4 ballot = subgroupBallot(intersect);
    count = subgroupBallotBitCount(ballot);
    if (gl_SubgroupInvocationID == 0)
    {
        id = atomicAdd(sh_Count, count);
    }
    id  = subgroupBroadcastFirst(id);
    id += subgroupBallotExclusiveBitCount(ballot);
#endif

    if (intersect)
    {
    #if !USE_SUBGROUP
        id = atomicAdd(sh_Count, 1);
    #endif

        payload.meshletIds[id] = globalThreadId;
    }

    barrier();
    
    count = sh_Count;
    
    {
        uint groupsX = count * TASK_MESHLET_SIZE;
        uint groupsY = TASK_MESHLET_SIZE;
        uint groupsZ = TASK_MESHLET_SIZE;
        EmitMeshTasksEXT(groupsX, groupsY, groupsZ);
    }
}

#else

#define FIELD_SIZE (TASK_GROUP_SIZE+1)

shared uint sh_Values[FIELD_SIZE][FIELD_SIZE][FIELD_SIZE];

layout (local_size_x = TASK_GROUP_SIZE, local_size_y = TASK_GROUP_SIZE, local_size_z = TASK_GROUP_SIZE) in;
void main()
{
    const uint localThreadIndex = gl_LocalInvocationIndex;
    const uvec3 workGroupId = gl_WorkGroupID;
    const uvec3 localThreadId = gl_LocalInvocationID;
    const uvec3 globalThreadId = gl_GlobalInvocationID;

    const vec3 bmin = _Constants.bmin.xyz;
    const vec3 bsize = _Constants.bsize.xyz;
    const uvec3 resolution = _Constants.resolution.xyz;

    vec3 step = bsize.xyz / vec3(resolution);
    vec3 pmin = bmin + step * vec3(globalThreadId);
    vec3 pmax = pmin + step;

    vec3 basePosition = bmin + step * vec3(workGroupId * TASK_GROUP_SIZE);

    const uint GroupSize = TASK_GROUP_SIZE * TASK_GROUP_SIZE * TASK_GROUP_SIZE;
    const uint FieldCount = FIELD_SIZE * FIELD_SIZE * FIELD_SIZE;
    for (uint i = localThreadIndex; i < FieldCount; i += GroupSize)
    {
    #if 0
        uint x = i % 5;
        uint y =(i / 5) % 5;
        uint z = i / 25;
    #else
        uint t = i / 5;
        uint x = i - t * 5;
        uint z = t / 5;
        uint y = t - z * 5;
    #endif

        vec3 p = basePosition + step * vec3(x, y, z);
        sh_Values[z][y][x] = Field(p) > 0 ? 0 : 1;
    }

    barrier();

    if (localThreadIndex == 0)
        sh_Count = 0;

    barrier();

    uint id = 0;
    uint count = 0;
    {
        uint cubeIndex = 0;
        uint x = localThreadId.x, y = localThreadId.y, z = localThreadId.z;
        cubeIndex |= (sh_Values[z + 0][y + 0][x + 0]);
        cubeIndex |= (sh_Values[z + 0][y + 0][x + 1] << 1);
        cubeIndex |= (sh_Values[z + 0][y + 1][x + 0] << 2);
        cubeIndex |= (sh_Values[z + 0][y + 1][x + 1] << 3);
        cubeIndex |= (sh_Values[z + 1][y + 0][x + 0] << 4);
        cubeIndex |= (sh_Values[z + 1][y + 0][x + 1] << 5);
        cubeIndex |= (sh_Values[z + 1][y + 1][x + 0] << 6);
        cubeIndex |= (sh_Values[z + 1][y + 1][x + 1] << 7);

        bool intersect = cubeIndex > 0 && cubeIndex < 0xFF;

        uvec4 ballot = subgroupBallot(intersect);
        count = subgroupBallotBitCount(ballot);
        if (gl_SubgroupInvocationID == 0)
            id = atomicAdd(sh_Count, count);
        id  = subgroupBroadcastFirst(id);
        id += subgroupBallotExclusiveBitCount(ballot);

        if (intersect)
        {
            payload.meshletIds[id] = globalThreadId;
        }
    }

    barrier();

    count = sh_Count;
    {
        uint groupsX = count;
        uint groupsY = 1;
        uint groupsZ = 1;
        EmitMeshTasksEXT(groupsX, groupsY, groupsZ);
    }
}

#endif
