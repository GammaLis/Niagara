#pragma once

#include "pch.h"
#include "Config.h"
#include "Utilities.h"


namespace Niagara
{
	struct Vertex
	{
		glm::vec3 p;
#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
		// glm::u16vec3 p;
		glm::u8vec4 n;
		glm::u16vec2 uv;
#else
		glm::vec3 n;
		glm::vec2 uv;
#endif

		static VkVertexInputBindingDescription GetBindingDescription()
		{
			// A vertex binding describes at which rate to load data from memory throughout the vertices.
			// It specifies the number of bytes between data entries and whether to move to the next data entry after 
			// each vertex or after each instance.
			VkVertexInputBindingDescription desc{};
			// All of the per-vertex data is packed together in one array, so we're only going to have 1 binding.
			desc.binding = 0;
			desc.stride = sizeof(Vertex);
			desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return desc;
		}

		static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescs(3);

			// float	VK_FORAMT_R32_SFLOAT
			// vec2		VK_FORMAT_R32G32_SFLOAT
			// vec3		VK_FORMAT_R32G32B32_SFLOAT
			// vec4		VK_FORMAT_R32G32B32A32_SFLOAT
			// ivec2	VK_FORMAT_R32G32_SINT
			// uvec4	VK_FORMAT_R32G32B32A32_UINT
			// double	VK_FORMAT_R64_SFLOAT

			uint32_t vertexAttribOffset = 0;

			attributeDescs[0].binding = 0;
			attributeDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescs[0].location = 0;
			attributeDescs[0].offset = offsetof(Vertex, p); // vertexAttribOffset 
			vertexAttribOffset += 3 * 4;

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
			attributeDescs[1].binding = 0;
			attributeDescs[1].format = VK_FORMAT_R8G8B8A8_UINT;
			attributeDescs[1].location = 1;
			attributeDescs[1].offset = offsetof(Vertex, n); // vertexAttribOffset;
			vertexAttribOffset += 4;

			attributeDescs[2].binding = 0;
			attributeDescs[2].format = VK_FORMAT_R16G16_SFLOAT;
			attributeDescs[2].location = 2;
			attributeDescs[2].offset = offsetof(Vertex, uv); // vertexAttribOffset;
			vertexAttribOffset += 2 * 2;

#else
			attributeDescs[1].binding = 0;
			attributeDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescs[1].location = 1;
			attributeDescs[1].offset = offsetof(Vertex, n); // vertexAttribOffset;
			vertexAttribOffset += 3 * 4;

			attributeDescs[2].binding = 0;
			attributeDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescs[2].location = 2;
			attributeDescs[2].offset = offsetof(Vertex, uv); // vertexAttribOffset;
			vertexAttribOffset += 2 * 4;
#endif

			return attributeDescs;
		}
	};

	/**
	* Meshlets
	* Each meshlet represents a variable number of vertices and primitives. There are no restrictions regarding the connectivity of
	* these primitives. However, they must stay below a maximum amount, specified within the shader code.
	* We recommend using up to 64 vertices and 126 primitives. The `6` in 126 is not a typo. The first generation hardware allocates
	* primitive indices in 128 byte granularity and needs to reserve 4 bytes for the primitive count. Therefore 3 * 126 + 4 maximizes
	* the fit into a 3 * 128 = 384 bytes block. Going beyond 126 triangles would allocate the next 128 bytes. 84 and 40 are other
	* maxima that work well for triangles.
	*/
	struct alignas(16) Meshlet
	{
		glm::vec4 boundingSphere; // xyz - center, w - radius
		glm::vec4 coneApex; // w - unused
		glm::vec4 cone; // xyz - cone direction, w - cosAngle
		// uint32_t vertices[MESHLET_MAX_VERTICES];
		// uint8_t indices[MESHLET_MAX_PRIMITIVES*3]; // up to MESHLET_MAX_PRIMITIVES triangles
		uint32_t vertexOffset;
		uint8_t vertexCount = 0;
		uint8_t triangleCount = 0;
	};

	struct MeshLod
	{
		uint32_t indexOffset;
		uint32_t indexCount;
		uint32_t meshletOffset;
		uint32_t meshletCount;
	};

	struct alignas(16) Mesh
	{
		glm::vec4 boundingSphere;

		uint32_t vertexOffset;
		uint32_t vertexCount;

		uint32_t lodCount;
		MeshLod lods[MESH_MAX_LODS];
	};

	struct Geometry
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<uint32_t> meshletData;
		std::vector<Meshlet> meshlets;

		std::vector<Mesh> meshes;
	};

	size_t BuildOptMeshlets(Geometry& result, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	bool LoadObj(std::vector<Vertex>& vertices, const char* path);
	bool LoadMesh(Geometry& result, const char* path, bool bBuildMeshlets = true, bool bIndexless = false);
}
