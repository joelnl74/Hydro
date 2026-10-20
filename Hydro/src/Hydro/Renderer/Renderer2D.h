#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/Texture2D.h"

namespace Hydro
{
	// Stats
	struct Statistics
	{
		// Geometry submitted this frame.
		uint32_t DrawCalls = 0;
		uint32_t QuadCount = 0;
		uint32_t LineCount = 0;

		// Batching.
		uint32_t BatchCount = 0;          // vertex batches used this frame (each holds up to MaxQuadsPerBatch quads)
		uint32_t BatchesAllocated = 0;    // batches kept alive for reuse
		uint32_t MaxQuadsPerBatch = 0;
		uint64_t VertexBytesUploaded = 0; // CPU -> GPU vertex upload this frame

		// Textures.
		uint32_t MaxTextureSlots = 0;     // textures one draw call can sample (slot 0 is the white texture)
		uint32_t TextureSlotsUsed = 0;    // slots filled this frame, summed over all texture sets of all views
		uint32_t TextureFlushes = 0;      // extra draw calls because a draw ran out of texture slots

		// GPU memory (from VMA).
		uint64_t GPUMemoryUsed = 0;
		uint64_t GPUMemoryFree = 0;

		// GPU time of the 2D render pass (timestamp queries; a couple of frames old). -1 = unsupported.
		float GPUTimeMs = -1.0f;
		const char* GPUName = "";

		// Frame / target.
		uint32_t ViewCount = 0;      // views rendered this frame
		uint32_t ViewportWidth = 0;  // size of the first view rendered this frame
		uint32_t ViewportHeight = 0;
		uint32_t FramesInFlight = 0;
		uint32_t FrameIndex = 0;

		uint32_t GetTotalVertexCount() const { return QuadCount * 4 + LineCount * 2; }
		uint32_t GetTotalIndexCount() const { return QuadCount * 6 + LineCount * 2; }
		uint32_t GetTriangleCount() const { return QuadCount * 2; }
	};

	// Draws batched quads into RenderViews. A part of Renderer: Renderer::Init / Shutdown start and
	// stop it, and Renderer::RenderFrame calls Render once per frame.
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		// Starts queuing quads for `view`, seen through viewProjection. Projections must come from
		// Camera (Camera::Perspective / Camera::Orthographic), which handles Vulkan's clip space.
		// Each view begun this frame gets its own 2D pass in Render, in the order they were begun.
		// Beginning the same view again in a frame adds to its quads and replaces its camera.
		static void BeginScene(RenderView& view, const glm::mat4& viewProjection);
		// cameraTransform is the camera's world transform (position and rotation, no scale).
		static void BeginScene(RenderView& view, const Camera& camera, const glm::mat4& cameraTransform);
		static void EndScene();

		// Queue a quad for the view between BeginScene and EndScene. Only writes CPU-side vertex
		// data. When the current batch is full a new batch is started automatically - there is no
		// hard limit on quads per frame.
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		// Textured quad: the texture, repeated tilingFactor times across the quad, multiplied by
		// tintColor. A null texture draws the white texture (a plain coloured quad).
		// One draw call samples up to MaxTextureSlots textures. The quad that needs one more starts a
		// new draw call with its own texture slots ("texture flush"), in the same vertex batch, so
		// there is no limit on textures per frame either. Draw order is kept.
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		// Records one render pass per view begun this frame (one vkCmdDrawIndexed per draw) into
		// the frame's command buffer. Called once per frame by Renderer::RenderFrame.
		static void Render(VkCommandBuffer commandBuffer);

		// Throw away this frame's quads without rendering (e.g. while the window is minimized).
		static void DiscardFrame();

		// Stats from the last frame that was rendered (all views together).
		static Statistics GetStats();

		// Textures one draw call can sample, including the white texture in slot 0.
		static uint32_t GetMaxTextureSlots();

	private:
		static void AllocateBatch(RenderViewData& view);
		static void NextBatch(RenderViewData& view);
		static void ResetBatches(RenderViewData& view);
		static void AllocateTextureSet(RenderViewData& view);
		static void NextTextureSet(RenderViewData& view);
		// Slot of `texture` in the view's current texture set; adds it, or starts a new set (and draw)
		// when the set is full.
		static float GetTextureSlot(RenderViewData& view, const Ref<Texture2D>& texture);
		// Starts a new draw at the current batch's write position with the current texture set.
		static void StartDraw(RenderViewData& view);
		static void SubmitQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& color);
		static void ApplyViewResize(RenderViewData& view);
		static void RecordView(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// Applies Renderer::GetSettings() that need a rebuild (wireframe), and recompiles the quad
		// shader when a source file changed (only with RendererSettings::ShaderHotReload).
		static void ApplySettings();
		static void ReloadShadersIfChanged();
		// Builds a new quad pipeline (optionally with a new shader) and swaps it in once the GPU is
		// idle. On failure the current one keeps drawing and false is returned.
		static bool RebuildQuadPipeline(bool recompileShader, bool wireframe);

		friend class Hydro::RenderView;
	};
}
