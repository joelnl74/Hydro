#include <hypch.h>

#include "VulkanVertexBuffer.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;

		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_VertexBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create vertex buffer!");
		 }
	}
}
