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
		None = 0,  // opaque: source overwrites the target
		Alpha,     // src * a + dst * (1 - a): sprites
		Additive,  // src * a + dst: glows and particles
		Multiply2x, // dst * src * 2 (0.5 keeps the target): applying a light buffer
		Darken,     // dst * (1 - a), colour ignored: shadows
	};

	struct PipelineSpecification
	{
		// Not owned; only read at creation, so the shader may be destroyed or replaced afterwards.
		VulkanShader* Shader = nullptr;
		// In set-index order; empty = none. Not owned: they must outlive the pipeline when BindDescriptorSets is used.
		std::vector<VulkanDescriptorBuilder*> DescriptorSets;
		// Used when DescriptorSets is empty: set layouts in set-index order, for sets that come from elsewhere.
		std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;

		VertexBufferLayout Layout;
		// The target's attachment formats (dynamic rendering), colour in attachment order; must match exactly.
		// Integer formats are never blended. Empty with a DepthFormat: a depth-only pipeline (shadow maps).
		std::vector<ImageFormat> ColorFormats = { ImageFormat::SRGB };
		ImageFormat DepthFormat = ImageFormat::None;
		// Bytes of push constants for the vertex and fragment stages; 0 = none. Keep it within 128 (the guaranteed minimum).
		uint32_t PushConstantSize = 0;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		// Depth bias (both 0 = off): Constant depth units plus Slope times the depth slope; negative = towards the camera.
		float DepthBiasConstant = 0.0f;
		float DepthBiasSlope = 0.0f;
		bool Wireframe = false;
		float LineWidth = 1.0f; // other values need the wideLines feature
		BlendMode Blend = BlendMode::None;
		// False: integer colour attachments (the entity ID) aren't written, so draws without an ID keep what earlier
		// draws wrote.
		bool WriteIntegerAttachments = true;
	};

	// A graphics pipeline and its layout, freed through Renderer::SubmitResourceFree so it can be replaced mid-frame.
	class VulkanPipeline
	{
	public:
		explicit VulkanPipeline(const PipelineSpecification& spec);
		~VulkanPipeline();

		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		// Also sets the viewport and scissor to `extent`.
		void Bind(VkCommandBuffer commandBuffer, VkExtent2D extent);
		// This frame's set of every DescriptorSets entry.
		void BindDescriptorSets(VkCommandBuffer commandBuffer);
		// Binds this frame's set of `descriptorSet` at `setIndex` instead; its layout must match the specification's.
		void BindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t setIndex, VulkanDescriptorBuilder& descriptorSet);
		// `set`'s layout must be the one this pipeline was built with for that index.
		void BindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t setIndex, VkDescriptorSet set);
		// From offset 0, at most PipelineSpecification::PushConstantSize bytes.
		void PushConstants(VkCommandBuffer commandBuffer, const void* data, uint32_t size);

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }
		// Shader is null here; set it again before building a pipeline from a copy.
		const PipelineSpecification& GetSpecification() const { return m_PipelineSpecification; }

	private:
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		PipelineSpecification m_PipelineSpecification;
	};
}


