#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Vulkan/VulkanBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanVertexLayout.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/Texture2D.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class VulkanDescriptorBuilder;
	struct QuadDraw;
	struct RenderViewData;

	// Writes quads into a view's vertex batches and draw lists, and records those lists. Owns the white texture
	// (slot 0 of every texture set) and the index buffer all batches share.
	class QuadBatcher
	{
	public:
		// Quads per batch (one vertex buffer each); more batches are made when a frame needs them.
		static constexpr uint32_t MaxQuads = 20000;
		static constexpr uint32_t MaxVertices = MaxQuads * 4;
		static constexpr uint32_t MaxIndices = MaxQuads * 6;

		static void Init();
		static void Shutdown();

		// The view's first batch and texture set.
		static void AddView(RenderViewData& view);
		// Empties the view's batches and draw lists for the next frame.
		static void Reset(RenderViewData& view);
		// Starts a new draw at the current batch's write position with the current texture set.
		static void StartDraw(RenderViewData& view);
		// The list quads go to now: the shadow casters, debug lines, overlay or the sprite draw mode's list.
		static std::vector<QuadDraw>& GetCurrentDraws(RenderViewData& view);
		// Corners counter-clockwise from the bottom-left. `shadowCorners`: where each corner looks up the sun shadow
		// map (null: the corners themselves).
		static void Submit(RenderViewData& view, const glm::vec3 (&corners)[4], const Ref<Texture2D>& texture,
			const TextureRect& textureRect,
			const glm::vec4& color, const glm::vec3 (*shadowCorners)[4] = nullptr);

		// Uploads the frame's batches and points its texture sets at their textures. Before the view's passes:
		// a descriptor set can't change once bound.
		static void Upload(RenderViewData& view, uint32_t frame);
		// Records one of the view's draw lists with pipelines for its attachments; nothing when empty.
		static void Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame, const std::vector<QuadDraw>& draws,
			SpriteDrawMode mode, bool overlay, bool debugLines = false);
		static void BindIndexBuffer(VkCommandBuffer commandBuffer);

		// Set 0 of the quad and mesh shaders: camera, texture array, forward lighting, lights, sun shadow map.
		static Scope<VulkanDescriptorBuilder> BuildDescriptorSet(const PerFrameBuffers& uniformBuffers,
			const PerFrameBuffers& lightingBuffers,
			const PerFrameBuffers& lightBuffers);
		static VertexBufferLayout GetVertexLayout();
		static const Ref<Texture2D>& GetWhiteTexture();
	};
}
