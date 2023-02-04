#include "Geometry.h"

#include "meshoptimizer.h"

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

namespace Niagara
{
#if 0

	void BuildMeshlets(Mesh& mesh)
	{
		std::vector<Meshlet> meshlets;
		Meshlet meshlet{};
		std::vector<uint8_t> meshletVertices(mesh.vertices.size(), 0xFF);
		uint8_t indexOffset = 0, vertexOffset = 0;
		for (size_t i = 0; i < mesh.indices.size(); i += 3)
		{
			uint32_t i0 = mesh.indices[i];
			uint32_t i1 = mesh.indices[i + 1];
			uint32_t i2 = mesh.indices[i + 2];

			uint8_t& v0 = meshletVertices[i0];
			uint8_t& v1 = meshletVertices[i1];
			uint8_t& v2 = meshletVertices[i2];

			if (meshlet.vertexCount + (v0 == 0xFF) + (v1 == 0xFF) + (v2 == 0xFF) > MESHLET_MAX_VERTICES ||
				meshlet.triangleCount >= MESHLET_MAX_PRIMITIVES)
			{
				mesh.meshlets.push_back(meshlet);
				for (uint8_t mv = 0; mv < meshlet.vertexCount; ++mv)
					meshletVertices[meshlet.vertices[mv]] = 0xFF;
				meshlet = {};
			}

			if (v0 == 0xFF)
			{
				v0 = meshlet.vertexCount;
				meshlet.vertices[meshlet.vertexCount++] = i0;
			}
			if (v1 == 0xFF)
			{
				v1 = meshlet.vertexCount;
				meshlet.vertices[meshlet.vertexCount++] = i1;
			}
			if (v2 == 0xFF)
			{
				v2 = meshlet.vertexCount;
				meshlet.vertices[meshlet.vertexCount++] = i2;
			}

			meshlet.indices[meshlet.triangleCount * 3 + 0] = v0;
			meshlet.indices[meshlet.triangleCount * 3 + 1] = v1;
			meshlet.indices[meshlet.triangleCount * 3 + 2] = v2;
			meshlet.triangleCount++;
		}

		if (meshlet.triangleCount > 0)
			mesh.meshlets.push_back(meshlet);
	}

	void BuildMeshletCones(Mesh& mesh)
	{
		for (auto& meshlet : mesh.meshlets)
		{
			glm::vec3 normals[MESHLET_MAX_PRIMITIVES];
			for (uint32_t i = 0; i < meshlet.triangleCount; ++i)
			{
				uint32_t i0 = meshlet.indices[3 * i + 0];
				uint32_t i1 = meshlet.indices[3 * i + 1];
				uint32_t i2 = meshlet.indices[3 * i + 2];

				const Vertex& v0 = mesh.vertices[meshlet.vertices[i0]];
				const Vertex& v1 = mesh.vertices[meshlet.vertices[i1]];
				const Vertex& v2 = mesh.vertices[meshlet.vertices[i2]];
#if 0
				glm::vec3 p0{ Niagara::ToFloat(v0.p.x),  Niagara::ToFloat(v0.p.y) , Niagara::ToFloat(v0.p.z) };
				glm::vec3 p1{ Niagara::ToFloat(v1.p.x),  Niagara::ToFloat(v1.p.y) , Niagara::ToFloat(v1.p.z) };
				glm::vec3 p2{ Niagara::ToFloat(v2.p.x),  Niagara::ToFloat(v2.p.y) , Niagara::ToFloat(v2.p.z) };
#else
				glm::vec3 p0 = v0.p, p1 = v1.p, p2 = v2.p;
#endif
				glm::vec3 n = glm::cross(p1 - p0, p2 - p0);

				normals[i] = Niagara::SafeNormalize(n);
			}

			glm::vec3 avgNormal{};
			for (uint32_t i = 0; i < meshlet.triangleCount; ++i)
				avgNormal += normals[i];
			float n = sqrtf(avgNormal.x * avgNormal.x + avgNormal.y * avgNormal.y + avgNormal.z * avgNormal.z);
			if (n < Niagara::EPS)
				avgNormal = glm::vec3(1.0f, 0.0f, 0.0f);
			else
				avgNormal = avgNormal / (n);

			float minDot = 1.0f;
			for (uint32_t i = 0; i < meshlet.triangleCount; ++i)
			{
				float dot = normals[i].x * avgNormal.x + normals[i].y * avgNormal.y + normals[i].z * avgNormal.z;
				if (dot < minDot)
					minDot = dot;
			}

			meshlet.cone = glm::vec4(avgNormal, minDot);
		}
	}

#endif


	bool LoadObj(std::vector<Vertex>& vertices, const char* path)
	{
		fastObjMesh* obj = fast_obj_read(path);

		if (obj == nullptr)
			return false;

		size_t vertexCount = 0, indexCount = 0;
		for (uint32_t i = 0; i < obj->face_count; ++i)
		{
			vertexCount += obj->face_vertices[i];
			indexCount += (obj->face_vertices[i] - 2) * 3; // 3 -> 3, 4 -> 6
		}

		// Currently, duplicated vertices, vertexCount == indexCount
		vertexCount = indexCount;

		vertices.resize(vertexCount);

		size_t vertexOffset = 0, indexOffset = 0;
		for (uint32_t i = 0; i < obj->face_count; ++i)
		{
			for (uint32_t j = 0; j < obj->face_vertices[i]; ++j)
			{
				fastObjIndex objIndex = obj->indices[indexOffset + j];

				// Triangulate polygon on the fly; offset - 3 is always the first polygon vertex
				if (j >= 3)
				{
					vertices[vertexOffset + 0] = vertices[vertexOffset - 3];
					vertices[vertexOffset + 1] = vertices[vertexOffset - 1];
					vertexOffset += 2;
				}

				Vertex& v = vertices[vertexOffset++];

				// P
				v.p.x = obj->positions[objIndex.p * 3 + 0];
				v.p.y = obj->positions[objIndex.p * 3 + 1];
				v.p.z = obj->positions[objIndex.p * 3 + 2];

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
				// P
				// v.p.x = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 0]);
				// v.p.y = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 1]);
				// v.p.z = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 2]);
				// N
				v.n.x = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 0] * 127.f + 127.5f);
				v.n.y = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 1] * 127.f + 127.5f);
				v.n.z = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 2] * 127.f + 127.5f);
				// UV
				v.uv.x = meshopt_quantizeHalf(obj->texcoords[objIndex.t * 2 + 0]);
				v.uv.y = meshopt_quantizeHalf(obj->texcoords[objIndex.t * 2 + 1]);
#else
				// N
				v.n.x = obj->normals[objIndex.n * 3 + 0];
				v.n.y = obj->normals[objIndex.n * 3 + 1];
				v.n.z = obj->normals[objIndex.n * 3 + 2];
				// UV
				v.uv.x = obj->texcoords[objIndex.t * 2 + 0];
				v.uv.y = obj->texcoords[objIndex.t * 2 + 1];
#endif
			}

			indexOffset += obj->face_vertices[i];
		}

		assert(vertexOffset == indexCount);

		fast_obj_destroy(obj);

		return true;
	}

	size_t BuildOptMeshlets(Geometry& result, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
		static const float ConeWeight = 0.25f;
		auto meshletCount = meshopt_buildMeshletsBound(indices.size(), MESHLET_MAX_VERTICES, MESHLET_MAX_PRIMITIVES);
		std::vector<meshopt_Meshlet> optMeshlets(meshletCount);
		std::vector<uint32_t> meshlet_vertices(meshletCount * MESHLET_MAX_VERTICES);
		std::vector<uint8_t> meshlet_triangles(meshletCount * MESHLET_MAX_PRIMITIVES * 3);
		meshletCount = meshopt_buildMeshlets(
			optMeshlets.data(),
			meshlet_vertices.data(),
			meshlet_triangles.data(),
			indices.data(),
			indices.size(),
			&vertices[0].p.x,
			vertices.size(),
			sizeof(Vertex),
			MESHLET_MAX_VERTICES,
			MESHLET_MAX_PRIMITIVES,
			ConeWeight);
		optMeshlets.resize(meshletCount);

		// Append meshlet data
		uint32_t vertexCount = 0, triangleCount = 0, indexGroupCount = 0;
		for (const auto& optMeshlet : optMeshlets)
		{
			vertexCount += optMeshlet.vertex_count;
			triangleCount += optMeshlet.triangle_count;
			indexGroupCount += Niagara::DivideAndRoundUp(optMeshlet.triangle_count * 3, 4);
		}
		uint32_t meshletDataOffset = static_cast<uint32_t>(result.meshletData.size());
#if USE_PACKED_PRIMITIVE_INDICES_NV
		result.meshletData.insert(result.meshletData.end(), vertexCount + indexGroupCount, 0);
#else
		result.meshletData.insert(result.meshletData.end(), vertexCount + triangleCount, 0);
#endif

		uint32_t meshletOffset = static_cast<uint32_t>(result.meshlets.size());
		result.meshlets.insert(result.meshlets.end(), meshletCount, {});

		for (uint32_t i = 0; i < meshletCount; ++i)
		{
			const auto& optMeshlet = optMeshlets[i];
			auto& meshlet = result.meshlets[meshletOffset + i];

			// Meshlet
			meshopt_Bounds bounds = meshopt_computeMeshletBounds(
				&meshlet_vertices[optMeshlet.vertex_offset],
				&meshlet_triangles[optMeshlet.triangle_offset],
				optMeshlet.triangle_count,
				&vertices[0].p.x,
				vertices.size(),
				sizeof(Vertex));
			meshlet.vertexCount = static_cast<uint8_t>(optMeshlets[i].vertex_count);
			meshlet.triangleCount = static_cast<uint8_t>(optMeshlets[i].triangle_count);
			meshlet.vertexOffset = meshletDataOffset;
			meshlet.boundingSphere = glm::vec4(bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius);
			meshlet.coneApex = glm::vec4(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[0], 0);
			meshlet.cone = glm::vec4(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2], bounds.cone_cutoff);

			// Vertex indices
			for (uint32_t j = 0; j < optMeshlet.vertex_count; ++j)
				result.meshletData[meshletDataOffset + j] = meshlet_vertices[optMeshlet.vertex_offset + j];

			meshletDataOffset += optMeshlet.vertex_count;

#if USE_PACKED_PRIMITIVE_INDICES_NV
			// Triangle indices (packed in 4 bytes)
			const uint32_t* indexGroups = reinterpret_cast<uint32_t*>(&meshlet_triangles[optMeshlet.triangle_offset]);
			uint32_t indexGroupCount = Niagara::DivideAndRoundUp(optMeshlet.triangle_count * 3, 4);
			for (uint32_t j = 0; j < indexGroupCount; ++j)
				result.meshletData[meshletDataOffset + j] = indexGroups[j];

			meshletDataOffset += indexGroupCount;

#else
			const uint8_t* indices = &meshlet_triangles[optMeshlet.triangle_offset];
			uint32_t triangleCount = optMeshlet.triangle_count;
			for (uint32_t j = 0; j < triangleCount; j++)
				result.meshletData[meshletDataOffset + j] = indices[j * 3 + 0] | (indices[j * 3 + 1] << 8) | (indices[j * 3 + 2] << 16);

			meshletDataOffset += triangleCount;

#endif
		}

		// Padding
#if 0
		if (meshletCount % TASK_GROUP_SIZE != 0)
		{
			size_t paddingCount = TASK_GROUP_SIZE - meshletCount % TASK_GROUP_SIZE;
			result.meshlets.insert(result.meshlets.end(), paddingCount, {});
			meshletCount += paddingCount;
		}
#endif

		return meshletCount;
	}

	bool LoadMesh(Geometry& result, const char* path, bool bBuildMeshlets, bool bIndexless)
	{
		std::vector<Vertex> triVertices;
		if (!LoadObj(triVertices, path))
			return false;

		size_t indexCount = triVertices.size();

		// FIXME: not used now
		if (bIndexless)
		{
			Mesh mesh{};
			mesh.vertexOffset = static_cast<uint32_t>(result.vertices.size());
			mesh.vertexCount = static_cast<uint32_t>(triVertices.size());

			result.vertices.insert(result.vertices.end(), triVertices.begin(), triVertices.end());
#if 0
			mesh.indices.resize(indexCount);
			for (size_t i = 0; i < indexCount; ++i)
				mesh.indices[i] = i;
#endif
		}
		else
		{
			std::vector<uint32_t> remap(indexCount);
			size_t vertexCount = meshopt_generateVertexRemap(remap.data(), nullptr, indexCount, triVertices.data(), indexCount, sizeof(Vertex));

			std::vector<Vertex> vertices(vertexCount);
			std::vector<uint32_t> indices(indexCount);

			meshopt_remapVertexBuffer(vertices.data(), triVertices.data(), indexCount, sizeof(Vertex), remap.data());
			meshopt_remapIndexBuffer(indices.data(), nullptr, indexCount, remap.data());

			meshopt_optimizeVertexCache(indices.data(), indices.data(), indexCount, vertexCount);
			meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indexCount, vertices.data(), vertexCount, sizeof(Vertex));

			// Mesh
			result.meshes.push_back({});
			auto& mesh = result.meshes.back();
			{
				// Vertices
				mesh.vertexOffset = static_cast<uint32_t>(result.vertices.size());
				mesh.vertexCount = static_cast<uint32_t>(vertexCount);
				result.vertices.insert(result.vertices.end(), vertices.begin(), vertices.end());

				// Bounding sphere
				glm::vec3 center{ 0.0f };
				for (const auto& vert : vertices)
					center += vert.p;
				center /= vertexCount;

				float radius = 0.0f;
				glm::vec3 tempVec{};
				for (const auto& vert : vertices)
				{
					tempVec = vert.p - center;
					radius = std::max(radius, glm::dot(tempVec, tempVec));
				}
				radius = sqrtf(radius);

				mesh.boundingSphere = glm::vec4(center, radius);

				// Lods
				size_t lodIndexCount = indexCount;
				std::vector<uint32_t> lodIndices = indices;
				while (mesh.lodCount < MESH_MAX_LODS)
				{
					auto& meshLod = mesh.lods[mesh.lodCount++];

					// Indices
					meshLod.indexOffset = static_cast<uint32_t>(result.indices.size());
					meshLod.indexCount = static_cast<uint32_t>(lodIndexCount);

					result.indices.insert(result.indices.end(), lodIndices.begin(), lodIndices.end());

					// Meshlets
					meshLod.meshletOffset = static_cast<uint32_t>(result.meshlets.size());
					meshLod.meshletCount = bBuildMeshlets ? static_cast<uint32_t>(BuildOptMeshlets(result, vertices, lodIndices)) : 0;

					// Simplify
					size_t nextTargetIndexCount = size_t(double(lodIndexCount * 0.5f)); // 0.75, 0.5
					size_t nextIndexCount = meshopt_simplify(lodIndices.data(), lodIndices.data(), lodIndexCount, &vertices[0].p.x, vertexCount, sizeof(Vertex), nextTargetIndexCount, 1e-2f);
					assert(nextIndexCount <= lodIndexCount);

					// We've reched the error bound
					if (nextIndexCount == lodIndexCount)
						break;

					lodIndexCount = nextIndexCount;
					lodIndices.resize(lodIndexCount);
					meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodIndexCount, vertexCount);
				}
			}
		}

		// Pad meshlets to TASK_GROUP_SIZE to allow shaders to over-read when running task shaders
#if 1
		size_t meshletCount = result.meshlets.size();
		if (meshletCount % TASK_GROUP_SIZE != 0)
		{
			size_t paddingCount = TASK_GROUP_SIZE - meshletCount % TASK_GROUP_SIZE;
			result.meshlets.insert(result.meshlets.end(), paddingCount, {});
			meshletCount += paddingCount;
		}
#endif

		// TODO: optimize the mesh for more efficient GPU rendering
		return true;
	}
}
