#pragma once

#include "pch.h"
#include "Shaders.h"
#include "Pipeline.h"
#include "Buffer.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace Niagara
{
    class Device;

    struct MetaBallData
    {
        glm::vec3 p;
        float r;
    };

    class Metaballs
    {
    public:
        static const uint32_t s_Resolution = 128;
        static const uint32_t s_MaxBallCount = 8;

        Shader metaballTaskShader;
        Shader metaballMeshShader;
        Shader metaballFragShader;

        GraphicsPipeline metaballPipeline;
        Buffer marchingCubesLookupBuffer;
        Buffer metaballBuffer;

        // Metaball data
        MetaBallData balls[s_MaxBallCount];
        
        void Init(const Device& device, const std::vector<VkFormat> &colorAttachmentFormats, VkFormat depthFormat = VK_FORMAT_UNDEFINED);
        void Destroy(const Device& device);
        
    };

    extern Metaballs g_Metaballs;
}
