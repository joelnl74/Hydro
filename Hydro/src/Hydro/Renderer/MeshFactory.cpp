#include "hypch.h"
#include "MeshFactory.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	// Adds a flat quad (corners counter-clockwise seen from where `normal` points). UVs come from the two
	// axes of the face that aren't along the normal, in world units.
	static void AddQuad(MeshData& mesh, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& normal)
	{
		const glm::vec3 corners[4] = { a, b, c, d };
		const uint32_t first = (uint32_t)mesh.Vertices.size();

		// Project on the plane the face lies in: drop the axis the normal points along most.
		const glm::vec3 n = glm::abs(normal);
		for (const glm::vec3& corner : corners)
		{
			MeshVertex vertex;
			vertex.Position = corner;
			vertex.Normal = normal;
			if (n.z >= n.x && n.z >= n.y)
			{
				vertex.UV = { corner.x, corner.y };
			}
			else if (n.x >= n.y)
			{
				vertex.UV = { corner.y * (normal.x > 0.0f ? 1.0f : -1.0f), corner.z };
			}
			else
			{
				vertex.UV = { corner.x * (normal.y < 0.0f ? 1.0f : -1.0f), corner.z };
			}
			// Images have v = 0 at the top: flip so a wall's texture stands upright.
			vertex.UV.y = -vertex.UV.y;
			mesh.Vertices.push_back(vertex);
		}

		const uint32_t quad[6] = { 0, 1, 2, 2, 3, 0 };
		for (uint32_t index : quad)
		{
			mesh.Indices.push_back(first + index);
		}
	}

	// A triangle, counter-clockwise seen from where `normal` points.
	static void AddTriangle(MeshData& mesh, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& normal)
	{
		const uint32_t first = (uint32_t)mesh.Vertices.size();
		const glm::vec3 corners[3] = { a, b, c };
		for (const glm::vec3& corner : corners)
		{
			MeshVertex vertex;
			vertex.Position = corner;
			vertex.Normal = normal;
			const bool side = std::abs(normal.x) > std::abs(normal.y);
			vertex.UV = side ? glm::vec2(corner.y * (normal.x > 0.0f ? 1.0f : -1.0f), -corner.z) : glm::vec2(corner.x, -corner.y);
			mesh.Vertices.push_back(vertex);
		}
		mesh.Indices.push_back(first);
		mesh.Indices.push_back(first + 1);
		mesh.Indices.push_back(first + 2);
	}

	// An axis-aligned block from min to max: six faces.
	static void AddBlock(MeshData& mesh, const glm::vec3& min, const glm::vec3& max, bool bottom)
	{
		const float x0 = min.x, y0 = min.y, z0 = min.z;
		const float x1 = max.x, y1 = max.y, z1 = max.z;
		AddQuad(mesh, { x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { 0.0f, 0.0f, 1.0f });   // top
		if (bottom)
		{
			AddQuad(mesh, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { x1, y0, z0 }, { 0.0f, 0.0f, -1.0f }); // bottom
		}
		AddQuad(mesh, { x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 }, { 0.0f, -1.0f, 0.0f });   // front (-Y)
		AddQuad(mesh, { x1, y1, z0 }, { x0, y1, z0 }, { x0, y1, z1 }, { x1, y1, z1 }, { 0.0f, 1.0f, 0.0f });    // back (+Y)
		AddQuad(mesh, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x1, y0, z1 }, { 1.0f, 0.0f, 0.0f });    // right (+X)
		AddQuad(mesh, { x0, y1, z0 }, { x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { -1.0f, 0.0f, 0.0f });   // left (-X)
	}

	static void Finish(MeshData& mesh)
	{
		mesh.SubMeshes = { { 0, (uint32_t)mesh.Indices.size(), 0 } };
	}

	MeshData MeshFactory::Plane(const glm::vec3& size)
	{
		MeshData mesh;
		const float x = size.x * 0.5f;
		const float y = size.y * 0.5f;
		AddQuad(mesh, { -x, -y, 0.0f }, { x, -y, 0.0f }, { x, y, 0.0f }, { -x, y, 0.0f }, { 0.0f, 0.0f, 1.0f });
		Finish(mesh);
		return mesh;
	}

	MeshData MeshFactory::Box(const glm::vec3& size)
	{
		MeshData mesh;
		AddBlock(mesh, { -size.x * 0.5f, -size.y * 0.5f, 0.0f }, { size.x * 0.5f, size.y * 0.5f, size.z }, true);
		Finish(mesh);
		return mesh;
	}

	MeshData MeshFactory::Ramp(const glm::vec3& size)
	{
		MeshData mesh;
		const float x0 = -size.x * 0.5f, x1 = size.x * 0.5f;
		const float y0 = -size.y * 0.5f, y1 = size.y * 0.5f;
		const float h = size.z;

		// The slope: from the front edge at the ground up to the back edge at full height.
		const glm::vec3 slopeNormal = glm::normalize(glm::vec3(0.0f, -h, size.y));
		AddQuad(mesh, { x0, y0, 0.0f }, { x1, y0, 0.0f }, { x1, y1, h }, { x0, y1, h }, slopeNormal);
		AddQuad(mesh, { x0, y0, 0.0f }, { x0, y1, 0.0f }, { x1, y1, 0.0f }, { x1, y0, 0.0f }, { 0.0f, 0.0f, -1.0f }); // bottom
		AddQuad(mesh, { x1, y1, 0.0f }, { x0, y1, 0.0f }, { x0, y1, h }, { x1, y1, h }, { 0.0f, 1.0f, 0.0f });        // back
		AddTriangle(mesh, { x1, y0, 0.0f }, { x1, y1, 0.0f }, { x1, y1, h }, { 1.0f, 0.0f, 0.0f });                   // right side
		AddTriangle(mesh, { x0, y1, 0.0f }, { x0, y0, 0.0f }, { x0, y1, h }, { -1.0f, 0.0f, 0.0f });                  // left side
		Finish(mesh);
		return mesh;
	}

	MeshData MeshFactory::Stairs(const glm::vec3& size, uint32_t steps)
	{
		MeshData mesh;
		steps = std::clamp(steps, 1u, 64u);
		const float depth = size.y / (float)steps;
		const float rise = size.z / (float)steps;
		const float y0 = -size.y * 0.5f;
		for (uint32_t i = 0; i < steps; i++)
		{
			// Step i covers the depth from its front edge to the back of the stairs, so the blocks together
			// are solid with no hidden faces inside except where they overlap (cheap and watertight).
			const glm::vec3 min{ -size.x * 0.5f, y0 + depth * (float)i, 0.0f };
			const glm::vec3 max{ size.x * 0.5f, y0 + depth * (float)(i + 1), rise * (float)(i + 1) };
			AddBlock(mesh, min, max, true);
		}
		Finish(mesh);
		return mesh;
	}

	MeshData MeshFactory::Cylinder(const glm::vec3& size, uint32_t segments)
	{
		MeshData mesh;
		segments = std::clamp(segments, 3u, 128u);
		const float rx = size.x * 0.5f;
		const float ry = size.y * 0.5f;
		const float h = size.z;
		const float step = glm::two_pi<float>() / (float)segments;
		const float circumference = glm::two_pi<float>() * std::sqrt((rx * rx + ry * ry) * 0.5f);

		for (uint32_t i = 0; i < segments; i++)
		{
			const float a0 = step * (float)i;
			const float a1 = step * (float)(i + 1);
			const glm::vec3 p0{ std::cos(a0) * rx, std::sin(a0) * ry, 0.0f };
			const glm::vec3 p1{ std::cos(a1) * rx, std::sin(a1) * ry, 0.0f };
			const glm::vec3 normal = glm::normalize(glm::vec3(std::cos((a0 + a1) * 0.5f) / std::max(rx, 1.0e-4f), std::sin((a0 + a1) * 0.5f) / std::max(ry, 1.0e-4f), 0.0f));

			// Side: UV u runs around the circumference in metres.
			const uint32_t first = (uint32_t)mesh.Vertices.size();
			const float u0 = circumference * (float)i / (float)segments;
			const float u1 = circumference * (float)(i + 1) / (float)segments;
			const glm::vec3 corners[4] = { p0, p1, p1 + glm::vec3(0.0f, 0.0f, h), p0 + glm::vec3(0.0f, 0.0f, h) };
			const glm::vec2 uvs[4] = { { u0, 0.0f }, { u1, 0.0f }, { u1, -h }, { u0, -h } };
			for (uint32_t c = 0; c < 4; c++)
			{
				MeshVertex vertex;
				vertex.Position = corners[c];
				vertex.Normal = normal;
				vertex.UV = uvs[c];
				mesh.Vertices.push_back(vertex);
			}
			const uint32_t quad[6] = { 0, 1, 2, 2, 3, 0 };
			for (uint32_t index : quad)
			{
				mesh.Indices.push_back(first + index);
			}

			// Caps: a fan around the centre.
			AddTriangle(mesh, { 0.0f, 0.0f, h }, p0 + glm::vec3(0.0f, 0.0f, h), p1 + glm::vec3(0.0f, 0.0f, h), { 0.0f, 0.0f, 1.0f });
			AddTriangle(mesh, { 0.0f, 0.0f, 0.0f }, p1, p0, { 0.0f, 0.0f, -1.0f });
		}
		Finish(mesh);
		return mesh;
	}
}
