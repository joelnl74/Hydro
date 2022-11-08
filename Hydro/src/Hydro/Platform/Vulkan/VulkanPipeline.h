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
		// Make this a vector of Ref shader.
		Ref<VulkanShader> VertexShader;
		Ref<VulkanShader> FragmentShader;

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
		void ShutDown();

		void Bind();

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }
		VkViewport GetViewPort() { return m_ViewPort; }
		VkRect2D GetRect2D() { return m_Scissor; }

	private:
		void CreateShaderDescriptorSetLayout(VkDevice& device);
	private:
		VkViewport m_ViewPort;
		VkRect2D m_Scissor;

		VkDescriptorSetLayout m_DescriptorSetLayout;

		PipelineSpecification m_Spec;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_GraphicsPipeline;
	};
}


