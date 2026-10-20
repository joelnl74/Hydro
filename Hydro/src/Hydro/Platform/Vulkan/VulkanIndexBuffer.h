#pragma once
#include "VulkanAllocator.h"

namespace Hydro
{
	// A host-visible buffer of uint32_t indices. A resource: owns its buffer (freed through
	// Renderer::SubmitResourceFree when it is destroyed) and is non-copyable; hold it in a Scope.
	class VulkanIndexBuffer
	{
	public:
		VulkanIndexBuffer(const void* data, uint32_t size);
		explicit VulkanIndexBuffer(uint32_t size);
		~VulkanIndexBuffer();

		VulkanIndexBuffer(const VulkanIndexBuffer&) = delete;
		VulkanIndexBuffer& operator=(const VulkanIndexBuffer&) = delete;

		void Bind(VkCommandBuffer commandBuffer);
		// Writes `size` bytes from the start of the buffer, while the GPU is not reading it.
		void SetData(const void* data, uint32_t size);

		uint32_t GetSize() const { return m_Size; }

	private:
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VmaAllocation m_Memory = nullptr;
		uint32_t m_Size = 0;
	};
}
