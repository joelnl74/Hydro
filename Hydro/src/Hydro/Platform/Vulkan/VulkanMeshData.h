#pragma once
#include "Hydro/Core/Base.h"
#include "VulkanBuffer.h"
#include "VulkanVertexLayout.h"

namespace Hydro
{
	// The backend half of Mesh: its GPU-only buffers.
	struct MeshBuffers
	{
		Scope<VulkanBuffer> Vertices;
		Scope<VulkanBuffer> Indices;
	};

	// The attributes of MeshVertex, in the order of mesh.vert's input locations.
	inline VertexBufferLayout GetMeshVertexLayout()
	{
		return VertexBufferLayout
		{
			{ "inPosition", ShaderDataType::Float3 },
			{ "inNormal", ShaderDataType::Float3 },
			{ "inUV", ShaderDataType::Float2 },
			{ "inColor", ShaderDataType::Float4 },
		};
	}
}
