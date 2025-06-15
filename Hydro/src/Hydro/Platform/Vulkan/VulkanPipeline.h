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
		// Ref<RenderPass> RenderPass;
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

		void Bind(VkCommandBuffer commandBuffer);
		void BindDescriptorSets(VkCommandBuffer commandBuffer);

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }
		VkViewport GetViewPort() { return m_ViewPort; }
		VkRect2D GetRect2D() { return m_Scissor; }

	private:
		VkPipelineLayout m_PipelineLayout;
		VkViewport m_ViewPort;
		VkRect2D m_Scissor;
		VkPipeline m_GraphicsPipeline;
		PipelineSpecification m_pipelineSpecification;
	};
}


