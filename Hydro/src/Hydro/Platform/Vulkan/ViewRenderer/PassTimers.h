#pragma once
#include "Hydro/Renderer/Renderer2D.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The lighting phases timed per lit view.
	enum class TimerPhase : uint32_t
	{
		LightBuffer = 0, // the whole light buffer pass, shadows included
		Shadows = 1,
		Composite = 2
	};

	// GPU timestamps and debug labels: the whole frame of views, the lighting phases of the first lit views and every
	// render graph pass. Results are read a couple of frames late, when the frame's fence has signalled; nothing waits.
	class PassTimers
	{
	public:
		static void Init();
		static void Shutdown();

		// After the frame's fence: reads what this frame index wrote last time, then resets and starts the frame's timer.
		static void BeginFrame(VkCommandBuffer commandBuffer, uint32_t frame);
		static void EndFrame(VkCommandBuffer commandBuffer, uint32_t frame);
		// Copies the last results and the GPU's name into `stats`.
		static void FillStats(Statistics& stats);

		// A named region in the command buffer (RenderDoc, Nsight, validation), when VK_EXT_debug_utils is on.
		static void BeginLabel(VkCommandBuffer commandBuffer, const char* name);
		static void EndLabel(VkCommandBuffer commandBuffer);

		// A begin/end timestamp pair around one pass, while the frame's slots last. BeginPass returns the timer for EndPass.
		static uint32_t BeginPass(VkCommandBuffer commandBuffer, uint32_t frame, const std::string& name);
		static void EndPass(VkCommandBuffer commandBuffer, uint32_t frame, uint32_t timer);
		static const std::vector<PassTiming>& GetPassTimings();

		// The lit view about to be recorded takes the next timed slot (the first few lit views are timed).
		static void BeginLitView();
		// Writes the begin or end timestamp of `phase` for the current lit view. Allowed inside a render pass.
		static void WritePhase(VkCommandBuffer commandBuffer, uint32_t frame, TimerPhase phase, bool end);
	};
}
