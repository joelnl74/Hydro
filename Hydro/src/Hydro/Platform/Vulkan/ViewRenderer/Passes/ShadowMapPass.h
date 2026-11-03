#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanRenderTarget;
	struct RenderViewData;

	// The sun's shadow map: one orthographic depth map per view, drawn before the scene from the meshes and the queued
	// sprite casters, then read by the lit shaders (set 0 binding 4). Skips itself without a shadow-casting sun.
	class ShadowMapPass : public RenderPass
	{
	public:
		static constexpr const char* PassName = "SunShadows";

		static void Init();
		static void Shutdown();
		// Whether `view` draws its map this frame; makes or resizes the map and works out its matrix. Before the uploads.
		static void Prepare(RenderViewData& view);
		// Binding 4's comparison sampler, and the 1 x 1 map it points at in views that draw none this frame.
		static VkSampler GetSampler();
		static const VulkanRenderTarget& GetEmptyMap();

		const char* GetName() const override { return "SunShadows"; }
		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override;
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;
	};
}
