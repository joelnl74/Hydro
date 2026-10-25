#pragma once
#include <glm/glm.hpp>

#include "Hydro/Renderer/RenderFeatures.h"

#include <cstdint>

namespace Hydro
{
	// How a 2D light fades out towards the edge of its radius (RendererSettings::LightFalloff).
	enum class LightFalloffStyle : uint8_t
	{
		Smooth = 0, // a continuous fade
		Banded      // RendererSettings::LightBands visible steps: the retro, pixel-art look
	};

	// Live options of the Renderer subsystem. Unlike a Specification these may change while the
	// engine runs: Renderer::SetSettings applies them from the next frame. They are saved with the
	// project (ProjectConfig::Renderer, in the .hyproj file).
	struct RendererSettings
	{
		// Colour the scene's offscreen target is cleared to each frame.
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		// Wait for the display's refresh (FIFO). Off: mailbox if available, else immediate.
		// Changing it recreates the swapchain.
		bool VSync = true;

		// Draw sprites as outlines. Needs the GPU's fillModeNonSolid feature
		// (RendererCapabilities::Wireframe); ignored with a warning otherwise.
		bool Wireframe = false;

		// 2D lighting. On: a view that has Light2DComponents is drawn lit (lights are drawn into a
		// light buffer that is multiplied over the scene). Off: lights are ignored and the scene is
		// drawn as it is. Views without lights are never changed.
		bool Lighting = true;
		// How lights fade out: Smooth, or Banded in LightBands steps (clamped to 2..32 when used).
		LightFalloffStyle LightFalloff = LightFalloffStyle::Smooth;
		uint32_t LightBands = 8;
		// Blob shadows (Shadow2DComponent) in lit views: they darken the light buffer. Needs Lighting.
		bool Shadows = true;

		// The project's render features (RenderFeatures.h): what every scene uses unless it overrides
		// them (SceneRenderSettings). Changing them rebuilds the views' render graphs. Missing from an
		// older .hyproj: the 2D preset, today's frame.
		RenderFeatureSettings Features;

		// Rebuild shaders when their source files change (checked a few times per second).
#ifdef HY_DIST
		bool ShaderHotReload = false;
#else
		bool ShaderHotReload = true;
#endif
	};

	// What the GPU supports, for UI that offers settings. Read-only, filled by Renderer::Init.
	// What the GPU and its driver can do, found when the renderer starts (Renderer::GetCapabilities).
	// Read-only. Optional features are true only when they were enabled on the device.
	struct RendererCapabilities
	{
		const char* GPUName = "";
		// The Vulkan version the driver reports for the GPU (at least 1.3: the renderer requires it).
		uint32_t VulkanVersionMajor = 0;
		uint32_t VulkanVersionMinor = 0;
		uint32_t VulkanVersionPatch = 0;
		uint32_t FramesInFlight = 0;

		// Required (Vulkan 1.3 core), always true once the renderer runs.
		bool DynamicRendering = false;
		bool Synchronization2 = false;

		// Optional, enabled when the GPU has them.
		bool Wireframe = false;          // fillModeNonSolid
		bool WideLines = false;          // line widths other than 1
		bool SamplerAnisotropy = false;

		// Supported but not enabled yet (bindless textures, later).
		bool DescriptorIndexing = false;

		// Limits that the 3D work depends on.
		uint32_t MaxPushConstantsSize = 0;        // bytes; Vulkan guarantees 128
		uint32_t MaxSampledImagesPerStage = 0;    // maxPerStageDescriptorSampledImages
		float MaxSamplerAnisotropy = 1.0f;
		bool DepthFormat32F = false;              // DEPTH32F usable as a depth attachment
		bool DepthFormat24Stencil8 = false;       // DEPTH24STENCIL8 usable as a depth attachment
	};
}
