#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Buffer.h"
#include "VulkanFrameContext.h"

#include <array>

namespace Hydro
{
	// A GPU buffer on VMA, from a BufferSpecification: vertex, index, uniform, storage or staging data.
	// HostVisible buffers stay mapped for their whole life and SetData writes through the mapping;
	// GpuOnly buffers get their contents once, at creation, through a staging buffer. A resource: owns
	// the buffer and its memory (freed through Renderer::SubmitResourceFree, so it can go at any point in
	// a frame) and is non-copyable; hold it in a Scope.
	class VulkanBuffer
	{
	public:
		// HostVisible: `data` (optional, Size bytes) is written at once. GpuOnly: `data` is required and is
		// copied with VulkanDevice::SubmitImmediate, which waits for the GPU: create these at load time.
		explicit VulkanBuffer(const BufferSpecification& specification, const void* data = nullptr);
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;

		// HostVisible only: copies `size` bytes to `offset` and flushes them (nothing to flush on coherent
		// memory). Only write data the GPU isn't reading: one buffer per frame in flight, written after that
		// frame's fence.
		void SetData(const void* data, size_t size, size_t offset = 0);
		// HostVisible only: the persistent mapping. Call Flush after writing through it.
		void* GetMappedData() const { return m_Mapped; }
		void Flush(size_t offset, size_t size);

		void BindAsVertexBuffer(VkCommandBuffer commandBuffer, uint32_t binding = 0) const;
		// Index buffers hold uint32_t indices.
		void BindAsIndexBuffer(VkCommandBuffer commandBuffer) const;

		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		size_t GetSize() const { return m_Specification.Size; }
		const BufferSpecification& GetSpecification() const { return m_Specification; }

	private:
		BufferSpecification m_Specification;
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = nullptr;
		void* m_Mapped = nullptr;
	};

	// One HostVisible buffer per frame in flight, so a frame never overwrites data an earlier frame still reads.
	using PerFrameBuffers = std::array<Scope<VulkanBuffer>, MAX_FRAMES_IN_FLIGHT>;

	// A HostVisible buffer of `specification` for each frame in flight.
	PerFrameBuffers CreatePerFrameBuffers(const BufferSpecification& specification);
}
