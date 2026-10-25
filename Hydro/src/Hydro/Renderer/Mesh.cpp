#include "hypch.h"
#include "Mesh.h"

#include "Hydro/Platform/Vulkan/VulkanMeshData.h"

#include <algorithm>
#include <cstddef>

namespace Hydro
{
	static_assert(sizeof(MeshVertex) == 48, "MeshVertex: must match GetMeshVertexLayout");
	static_assert(offsetof(MeshVertex, Normal) == 12 && offsetof(MeshVertex, UV) == 24 && offsetof(MeshVertex, Color) == 32,
		"MeshVertex: must match GetMeshVertexLayout");

	Mesh::Mesh(const MeshData& data, const std::string& debugName)
		: m_Buffers(CreateScope<MeshBuffers>()), m_SubMeshes(data.SubMeshes)
	{
		HY_CORE_ASSERT(data.Vertices.empty() == false && data.Indices.empty() == false, "Mesh: no vertices or indices");
		m_VertexCount = (uint32_t)data.Vertices.size();
		m_IndexCount = (uint32_t)data.Indices.size();
		if (m_SubMeshes.empty())
		{
			m_SubMeshes.push_back({ 0, m_IndexCount, 0 });
		}

		m_BoundsMin = data.Vertices.front().Position;
		m_BoundsMax = m_BoundsMin;
		for (const MeshVertex& vertex : data.Vertices)
		{
			m_BoundsMin = glm::min(m_BoundsMin, vertex.Position);
			m_BoundsMax = glm::max(m_BoundsMax, vertex.Position);
		}

		// Static: GPU-only memory, uploaded once through a staging buffer.
		BufferSpecification vertexSpecification;
		vertexSpecification.Usage = BufferUsage::Vertex;
		vertexSpecification.Memory = BufferMemory::GpuOnly;
		vertexSpecification.Size = data.Vertices.size() * sizeof(MeshVertex);
		vertexSpecification.DebugName = debugName + " vertices";
		m_Buffers->Vertices = CreateScope<VulkanBuffer>(vertexSpecification, data.Vertices.data());

		BufferSpecification indexSpecification;
		indexSpecification.Usage = BufferUsage::Index;
		indexSpecification.Memory = BufferMemory::GpuOnly;
		indexSpecification.Size = data.Indices.size() * sizeof(uint32_t);
		indexSpecification.DebugName = debugName + " indices";
		m_Buffers->Indices = CreateScope<VulkanBuffer>(indexSpecification, data.Indices.data());
	}

	// Here, where MeshBuffers is complete. The buffers free themselves through the deletion queue.
	Mesh::~Mesh() = default;
}
