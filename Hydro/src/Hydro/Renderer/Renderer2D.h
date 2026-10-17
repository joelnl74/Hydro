#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "Hydro/Renderer/Camera.h"

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
		uint32_t BatchCount = 0;          // batches used this frame (= quad draw calls)
		uint32_t BatchesAllocated = 0;    // batches kept alive for reuse
		uint32_t MaxQuadsPerBatch = 0;
		uint64_t VertexBytesUploaded = 0; // CPU -> GPU vertex upload this frame

		// GPU memory (from VMA).
		uint64_t GPUMemoryUsed = 0;
		uint64_t GPUMemoryFree = 0;

		// GPU time of the 2D render pass (timestamp queries; a couple of frames old). -1 = unsupported.
		float GPUTimeMs = -1.0f;
		const char* GPUName = "";

		// Frame / target.
		uint32_t ViewportWidth = 0;
		uint32_t ViewportHeight = 0;
		uint32_t FramesInFlight = 0;
		uint32_t FrameIndex = 0;

		uint32_t GetTotalVertexCount() const { return QuadCount * 4 + LineCount * 2; }
		uint32_t GetTotalIndexCount() const { return QuadCount * 6 + LineCount * 2; }
		uint32_t GetTriangleCount() const { return QuadCount * 2; }
	};

	class Renderer2D
	{
	public:
		static void Init(uint32_t width, uint32_t height);
		static void Shutdown();

		// The camera this frame's quads are seen through. Call before submitting; it applies to the whole
		// frame (there is one 2D pass), and stays set until the next BeginScene. Projections must come
		// from Camera (Camera::Perspective / Camera::Orthographic), which handles Vulkan's clip space.
		static void BeginScene(const glm::mat4& viewProjection);
		// cameraTransform is the camera's world transform (position and rotation, no scale).
		static void BeginScene(const Camera& camera, const glm::mat4& cameraTransform);

		// Queue a quad for this frame. Only writes CPU-side vertex data, so it can be
		// called any time before Render(). When the current batch is full a new batch
		// is started automatically - there is no hard limit on quads per frame.
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);

		// Records the whole 2D frame (ONE render pass, one vkCmdDrawIndexed per batch) into the
		// frame's command buffer. Called once per frame by Renderer::RenderFrame.
		static void Render(VkCommandBuffer commandBuffer);

		// Throw away this frame's quads without rendering (e.g. while the window is minimized).
		static void DiscardFrame();

		// Size of the offscreen image the scene is rendered into (e.g. the editor viewport
		// panel). Applied at the start of the next Render(); 0 is ignored.
		static void SetViewportSize(uint32_t width, uint32_t height);

		static VkDescriptorSet GetCompositeDescriptorSet();

		// Stats from the last frame that was rendered.
		static Statistics GetStats();

	private:
		static void AllocateBatch();
		static void NextBatch();
		static void ResetBatches();
		static void ApplyViewportResize();
		// Applies Renderer::GetSettings() that need a rebuild (wireframe), and recompiles the quad
		// shader when a source file changed (only with RendererSettings::ShaderHotReload).
		static void ApplySettings();
		static void ReloadShadersIfChanged();
		// Builds a new quad pipeline (optionally with a new shader) and swaps it in once the GPU is
		// idle. On failure the current one keeps drawing and false is returned.
		static bool RebuildQuadPipeline(bool recompileShader, bool wireframe);
	};

}
