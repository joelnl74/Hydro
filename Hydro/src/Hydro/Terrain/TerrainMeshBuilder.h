#pragma once
#include "Hydro/Renderer/Mesh.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

// Internal to the terrain's meshing (TerrainMesher, TerrainDecor): how triangles are collected.
namespace Hydro
{
	// Triangles per sub-mesh slot, flat shaded (three vertices each, the face's normal), joined into one MeshData at
	// the end with the slots in order.
	struct TerrainMeshBuilder
	{
		std::map<uint32_t, std::vector<MeshVertex>> Slots;

		// `shading`: the normal to light it with instead of the face's (decor sprites are lit like the ground).
		void Triangle(uint32_t slot, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec2& uv0, const glm::vec2& uv1,
			const glm::vec2& uv2, const glm::vec3* shading = nullptr)
		{
			const glm::dvec3 a = glm::dvec3(p1) - glm::dvec3(p0);
			const glm::dvec3 b = glm::dvec3(p2) - glm::dvec3(p0);
			const glm::dvec3 n = glm::cross(a, b);
			const double length = glm::length(n);
			if (length < 1e-10)
			{
				return;
			}
			const glm::vec3 normal = shading != nullptr ? *shading : glm::vec3(n / length);
			std::vector<MeshVertex>& vertices = Slots[slot];
			const glm::vec3 positions[3] = { p0, p1, p2 };
			const glm::vec2 uvs[3] = { uv0, uv1, uv2 };
			for (int k = 0; k < 3; k++)
			{
				MeshVertex vertex;
				vertex.Position = positions[k];
				vertex.Normal = normal;
				vertex.UV = uvs[k];
				vertices.push_back(vertex);
			}
		}

		// Counter-clockwise seen from the front.
		void Quad(uint32_t slot, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec2& uv0,
			const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3, const glm::vec3* shading = nullptr)
		{
			Triangle(slot, p0, p1, p2, uv0, uv1, uv2, shading);
			Triangle(slot, p0, p2, p3, uv0, uv2, uv3, shading);
		}

		// A box standing on `base` (sunk 1 cm), its length along `axis` turned by `yaw`; UVs in metres (x + z, -(y + z)).
		void Box(uint32_t slot, const glm::vec3& base, const glm::vec2& axis, float yaw, float width, float depth, float height)
		{
			const float c = std::cos(yaw);
			const float s = std::sin(yaw);
			const glm::vec2 ax(axis.x * c - axis.y * s, axis.x * s + axis.y * c);
			const glm::vec2 ay(-ax.y, ax.x);
			glm::vec3 corners[8];
			for (int k = 0; k < 8; k++)
			{
				const float sx = (k & 1) != 0 ? 0.5f : -0.5f;
				const float sy = (k & 2) != 0 ? 0.5f : -0.5f;
				const float sz = (k & 4) != 0 ? height : 0.0f;
				corners[k] = glm::vec3(base.x + ax.x * sx * width + ay.x * sy * depth, base.y + ax.y * sx * width + ay.y * sy * depth, base.z + sz - 0.01f);
			}
			// Corner index: x + 2y + 4z; faces counter-clockwise from outside.
			static const int faces[6][4] = { { 4, 5, 7, 6 }, { 0, 2, 3, 1 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 1, 3, 7, 5 }, { 0, 4, 6, 2 } };
			for (const auto& face : faces)
			{
				glm::vec3 p[4];
				glm::vec2 uv[4];
				for (int k = 0; k < 4; k++)
				{
					p[k] = corners[face[k]];
					uv[k] = glm::vec2(p[k].x + p[k].z, -(p[k].y + p[k].z));
				}
				Quad(slot, p[0], p[1], p[2], p[3], uv[0], uv[1], uv[2], uv[3]);
			}
		}

		void AppendTo(MeshData& data, const char* name) const
		{
			for (const auto& [slot, vertices] : Slots)
			{
				if (vertices.empty())
				{
					continue;
				}
				SubMesh subMesh;
				subMesh.FirstIndex = (uint32_t)data.Indices.size();
				subMesh.IndexCount = (uint32_t)vertices.size();
				subMesh.MaterialSlot = slot;
				subMesh.SourceName = name;
				const uint32_t first = (uint32_t)data.Vertices.size();
				data.Vertices.insert(data.Vertices.end(), vertices.begin(), vertices.end());
				for (uint32_t k = 0; k < (uint32_t)vertices.size(); k++)
				{
					data.Indices.push_back(first + k);
				}
				data.SubMeshes.push_back(subMesh);
			}
		}
	};

	// A builder that keeps only positions, welded, for the collision mesh: triangles that share an edge share its
	// vertices, so Jolt sees which edges are inside a flat area and characters don't catch on them.
	struct TerrainCollisionBuilder
	{
		std::vector<glm::vec3>& Positions;
		std::vector<uint32_t>& Indices;
		std::unordered_map<uint64_t, uint32_t> Welded;

		uint32_t Vertex(const glm::vec3& p)
		{
			// 0.1 mm grid: crossing points are computed the same way on both sides, so they land on the same key.
			const int64_t x = (int64_t)std::llround((double)p.x * 10000.0);
			const int64_t y = (int64_t)std::llround((double)p.y * 10000.0);
			const int64_t z = (int64_t)std::llround((double)p.z * 10000.0);
			const uint64_t key = ((uint64_t)(x & 0x1FFFFF) << 42) | ((uint64_t)(y & 0x1FFFFF) << 21) | (uint64_t)(z & 0x1FFFFF);
			auto found = Welded.find(key);
			if (found != Welded.end())
			{
				return found->second;
			}
			const uint32_t index = (uint32_t)Positions.size();
			Positions.push_back(p);
			Welded.emplace(key, index);
			return index;
		}

		void Triangle(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2)
		{
			const uint32_t a = Vertex(p0);
			const uint32_t b = Vertex(p1);
			const uint32_t c = Vertex(p2);
			if (a == b || b == c || a == c)
			{
				return;
			}
			Indices.push_back(a);
			Indices.push_back(b);
			Indices.push_back(c);
		}
	};

}
