#pragma once
#include <vulkan/vulkan.h>
#include "VulkanVertexBuffer.h"
#include "VulkanShader.h"
#include "VulkanDescriptorBuilder.h"

#include <vector>

namespace Hydro
{
	enum class PrimitiveTopology
	{
		None = 0,
		Points,
		Lines,
		Triangles,
	};

	enum class BlendMode
	{
		None = 0,  // Opaque: source overwrites the target.
		Alpha,     // Standard alpha blending (src * a + dst * (1 - a)). Use for sprites.
		Additive,  // src * a + dst. Use for glows and particles.
	};

	struct PipelineSpecification
	{
		Ref<VulkanShader> shader;
		// Descriptor sets in set-index order (element 0 is set = 0). Empty = the pipeline uses none.
		std::vector<Ref<VulkanDescriptorBuilder>> DescriptorSets;

		VertexBufferLayout Layout;
		// Render pass the pipeline draws into. VK_NULL_HANDLE = the swapchain's present pass.
		VkRenderPass RenderPass = VK_NULL_HANDLE;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		bool Wireframe = false;
		float LineWidth = 1.0f; // Values other than 1 need the GPU's wideLines feature.
		BlendMode Blend = BlendMode::None;
	};

	class VulkanPipeline
	{
	public:
		VulkanPipeline(const PipelineSpecification& spec);
		void Destroy();

		// Binds the pipeline and sets viewport/scissor to cover `extent` (the current render target).
		void Bind(VkCommandBuffer commandBuffer, VkExtent2D extent);
		// Binds this frame's set of every DescriptorSets entry. Does nothing when there are none.
		void BindDescriptorSets(VkCommandBuffer commandBuffer);
		// Binds this frame's set of `descriptorSet` at set index `setIndex` instead of the one in the
		// specification. Its layout must be defined identically to that entry's (e.g. built the same way).
		void BindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t setIndex, VulkanDescriptorBuilder& descriptorSet);

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }
		const PipelineSpecification& GetSpecification() const { return m_pipelineSpecification; }

	private:
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		PipelineSpecification m_pipelineSpecification;
	};
}


