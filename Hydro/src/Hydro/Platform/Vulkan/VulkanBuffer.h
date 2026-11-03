#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Buffer.h"
#include "VulkanFrameContext.h"

#include <array>

namespace Hydro
{
	// A GPU buffer on VMA. HostVisible buffers stay mapped (SetData writes through the mapping); GpuOnly buffers get
	// their contents once, at creation, through a staging buffer. Freed through Renderer::SubmitResourceFree.
	class VulkanBuffer
	{
	public:
		// HostVisible: optional `data` is written at once. GpuOnly: `data` is required and the upload waits for the GPU,
		// so create these at load time.
		explicit VulkanBuffer(const BufferSpecification& specification, const void* data = nullptr);
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;

		// HostVisible only: copies and flushes. Write only data the GPU isn't reading (one buffer per frame in flight).
		void SetData(const void* data, size_t size, size_t offset = 0);
		// HostVisible only. Call Flush after writing through it.
		void* GetMappedData() const { return m_Mapped; }
		void Flush(size_t offset, size_t size);

		void BindAsVertexBuffer(VkCommandBuffer commandBuffer, uint32_t binding = 0) const;
		// uint32_t indices.
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

	PerFrameBuffers CreatePerFrameBuffers(const BufferSpecification& specification);
}
