#pragma once
#include <vk_mem_alloc.h>

#include <vector>

namespace Hydro
{
	// One uniform buffer per frame in flight, so a frame never overwrites data an earlier frame still
	// reads. A resource: owns its buffers (freed through Renderer::SubmitResourceFree when it is
	// destroyed) and is non-copyable; hold it in a Scope.
	class VulkanUniformBuffer
	{
	public:
		explicit VulkanUniformBuffer(uint32_t size);
		~VulkanUniformBuffer();

		VulkanUniformBuffer(const VulkanUniformBuffer&) = delete;
		VulkanUniformBuffer& operator=(const VulkanUniformBuffer&) = delete;

		// Writes the buffer of frame in flight `frameIndex` (after that frame's fence).
		void Update(const void* data, uint32_t frameIndex, uint32_t size);

		const std::vector<VkBuffer>& GetVKBuffers() const { return m_UniformBuffers; }
		uint32_t GetSize() const { return m_Size; }

	private:
		std::vector<VkBuffer> m_UniformBuffers;
		std::vector<VmaAllocation> m_VmaAllocations;
		uint32_t m_Size = 0;
	};
}
