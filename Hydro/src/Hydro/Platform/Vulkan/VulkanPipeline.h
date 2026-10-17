#pragma once
#include <vulkan/vulkan.h>
#include "VulkanVertexBuffer.h"
#include "VulkanShader.h"

namespace Hydro
{
	enum class PrimitiveTopology
	{
		None = 0,
		Points,
		Lines,
		Triangles,
	};

	struct PipelineSpecification
	{
		Ref<VulkanShader> shader;
		Ref<VulkanDescriptorBuilder> descriptorSet;

		VertexBufferLayout Layout;
		// Render pass the pipeline draws into. VK_NULL_HANDLE = the swapchain's present pass.
		VkRenderPass RenderPass = VK_NULL_HANDLE;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		bool Wireframe = false;
		float LineWidth = 1.0f;
	};

	class VulkanPipeline
	{
	public:
		VulkanPipeline(const PipelineSpecification& spec);
		void Destroy();

		// Binds the pipeline and sets viewport/scissor to cover `extent` (the current render target).
		void Bind(VkCommandBuffer commandBuffer, VkExtent2D extent);
		void BindDescriptorSets(VkCommandBuffer commandBuffer);

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }

	private:
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		PipelineSpecification m_pipelineSpecification;
	};
}


