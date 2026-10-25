#pragma once
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// One vertex of a mesh: Z-up, metres. The colour multiplies the material (white for built-in shapes).
	struct MeshVertex
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		glm::vec2 UV{ 0.0f };
		glm::vec4 Color{ 1.0f };
	};

	// A range of a mesh's indices drawn with one material (MeshComponent::Materials[MaterialSlot]).
	struct SubMesh
	{
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t MaterialSlot = 0;
	};

	// A mesh on the CPU: what MeshFactory and (M5) the glTF importer make, and what Mesh uploads.
	struct MeshData
	{
		std::vector<MeshVertex> Vertices;
		std::vector<uint32_t> Indices; // triangles, counter-clockwise seen from outside
		std::vector<SubMesh> SubMeshes;
	};

	struct MeshBuffers; // Platform/Vulkan/VulkanMeshData.h

	// A static mesh on the GPU: vertex and index buffers in GPU-only memory (uploaded once, at creation),
	// its sub-meshes and local bounds. Shared through Ref<Mesh> (several entities may draw it). Frees its
	// buffers through the deletion queue when the last Ref goes.
	class Mesh
	{
	public:
		Mesh(const MeshData& data, const std::string& debugName);
		~Mesh();

		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;

		const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
		const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
		const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }
		uint32_t GetVertexCount() const { return m_VertexCount; }
		uint32_t GetIndexCount() const { return m_IndexCount; }

		const MeshBuffers& GetBuffers() const { return *m_Buffers; }

	private:
		Scope<MeshBuffers> m_Buffers;
		std::vector<SubMesh> m_SubMeshes;
		glm::vec3 m_BoundsMin{ 0.0f };
		glm::vec3 m_BoundsMax{ 0.0f };
		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;
	};
}
