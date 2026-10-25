#pragma once
#include <vulkan/vulkan.h>
#include "VulkanVertexLayout.h"
#include "VulkanShader.h"
#include "VulkanDescriptorBuilder.h"
#include "Hydro/Renderer/Image.h"

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
		Multiply2x, // dst * src * 2: a src of 0.5 leaves the target as it is, 1 doubles it. Use to apply a light buffer.
		Darken,     // dst * (1 - a): the source's alpha darkens the target, its colour is ignored. Use for shadows.
	};

	struct PipelineSpecification
	{
		// Not owned. Only read while the pipeline is created (shader modules aren't needed after that),
		// so the shader may be destroyed or replaced afterwards.
		VulkanShader* Shader = nullptr;
		// Descriptor sets in set-index order (element 0 is set = 0). Empty = the pipeline uses none.
		// Not owned: their layouts are read at creation; BindDescriptorSets binds them, so they must
		// outlive the pipeline when that is used.
		std::vector<VulkanDescriptorBuilder*> DescriptorSets;
		// Used instead when DescriptorSets is empty: the set layouts in set-index order (from
		// VulkanDescriptorAllocator::GetLayout), for pipelines whose sets come from elsewhere (materials).
		std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;

		VertexBufferLayout Layout;
		// The attachments the pipeline draws into (dynamic rendering, no VkRenderPass): the colour formats
		// in attachment order and the depth format, as the target reports them
		// (VulkanRenderTarget::GetPipelineColorFormats / GetPipelineDepthFormat). Must match the target
		// exactly. Integer formats (R32I, the entity ID) are never blended.
		std::vector<ImageFormat> ColorFormats = { ImageFormat::SRGB };
		ImageFormat DepthFormat = ImageFormat::None;
		// Bytes of push constants, visible to the vertex and fragment stages. 0 = none. At most
		// RendererCapabilities::MaxPushConstantsSize; keep it within 128, the guaranteed minimum.
		uint32_t PushConstantSize = 0;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		// Depth bias (both 0 = off): moves the drawn depth by Constant units of the depth format plus Slope
		// times the polygon's depth slope. Negative = towards the camera (forward Z). Ground decals use it.
		float DepthBiasConstant = 0.0f;
		float DepthBiasSlope = 0.0f;
		bool Wireframe = false;
		float LineWidth = 1.0f; // Values other than 1 need the GPU's wideLines feature.
		BlendMode Blend = BlendMode::None;
		// Integer colour attachments (the entity ID) are never blended. False: they aren't written at all
		// (write mask 0), so draws that must not change them (particles, overlays, the light composite,
		// shaders without an ID output) keep what earlier draws wrote.
		bool WriteIntegerAttachments = true;
	};

	// A graphics pipeline and its layout. A resource: owns both (freed through
	// Renderer::SubmitResourceFree when it is destroyed, so a pipeline can be replaced mid-frame
	// without waiting for the GPU) and is non-copyable; hold it in a Scope.
	class VulkanPipeline
	{
	public:
		explicit VulkanPipeline(const PipelineSpecification& spec);
		~VulkanPipeline();

		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		// Binds the pipeline and sets viewport/scissor to cover `extent` (the current render target).
		void Bind(VkCommandBuffer commandBuffer, VkExtent2D extent);
		// Binds this frame's set of every DescriptorSets entry. Does nothing when there are none.
		void BindDescriptorSets(VkCommandBuffer commandBuffer);
		// Binds this frame's set of `descriptorSet` at set index `setIndex` instead of the one in the
		// specification. Its layout must be defined identically to that entry's (e.g. built the same way).
		void BindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t setIndex, VulkanDescriptorBuilder& descriptorSet);
		// Binds `set` (e.g. one from VulkanDescriptorAllocator::AllocateFrame) at `setIndex`; its layout must be
		// the one this pipeline was built with for that set.
		void BindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t setIndex, VkDescriptorSet set);
		// Writes `size` bytes of push constants from offset 0 (at most PipelineSpecification::PushConstantSize).
		void PushConstants(VkCommandBuffer commandBuffer, const void* data, uint32_t size);

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }
		// Shader is null here: it is only used while the pipeline is created. Set it again before
		// building a new pipeline from a copy of this specification.
		const PipelineSpecification& GetSpecification() const { return m_pipelineSpecification; }

	private:
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		PipelineSpecification m_pipelineSpecification;
	};
}


