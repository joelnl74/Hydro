#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace Hydro
{
	// What a GPU buffer is for (Vulkan usage flags follow from it).
	enum class BufferUsage : uint8_t
	{
		Vertex = 0,
		Index,   // uint32_t indices
		Uniform,
		Storage,
		Staging  // the CPU side of an upload to a GpuOnly buffer
	};

	// Where a GPU buffer lives.
	enum class BufferMemory : uint8_t
	{
		GpuOnly = 0,  // fastest for the GPU; filled once, from a staging buffer, at load time
		HostVisible   // written by the CPU through a persistent mapping (per-frame data)
	};

	// Describes a GPU buffer (Platform/Vulkan/VulkanBuffer). A Parameter Object: no Vulkan types.
	struct BufferSpecification
	{
		BufferUsage Usage = BufferUsage::Vertex;
		BufferMemory Memory = BufferMemory::HostVisible;
		size_t Size = 0; // bytes
		std::string DebugName = "Buffer";
	};
}
