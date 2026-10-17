#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

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
		static void Init();
		static void ShutDown();

		// Queue a quad for this frame. Only writes CPU-side vertex data, so it can be
		// called any time before Render(). When the current batch is full a new batch
		// is started automatically - there is no hard limit on quads per frame.
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);

		// Records ONE command buffer with ONE render pass for the whole frame and issues
		// one vkCmdDrawIndexed per batch, then submits. Call exactly once per frame.
		static void Render();

		static void CreateCompositeImageView();
		static VkDescriptorSet GetCompositeDescriptorSet();

		// Stats from the last frame that was rendered.
		static Statistics GetStats();

	private:
		static void AllocateBatch();
		static void NextBatch();
		static void ResetBatches();
	};

}
