#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

#include <string>

namespace Hydro
{
	class VulkanPipeline;

	// The 2D light buffer: lights and blob shadows drawn into a per-view RGBA16F buffer before the scene, then multiplied
	// over the scene after the sprites. Optional: without its shaders every view is drawn unlit.
	class Light2DPass : public RenderPass
	{
	public:
		static void Init();
		static void Shutdown();
		// Null when the light shaders are unavailable.
		static VulkanPipeline* GetCompositePipeline();
		static void OnShaderReloaded(const std::string& name);

		const char* GetName() const override { return "LightBuffer2D"; }
		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override;
		void Setup(RenderGraphBuilder& builder) override;
		// The light buffer; skips itself when lighting is off, the view has no lights, or the shaders are unavailable.
		void Record(RenderPassContext& context) override;

	private:
		void RecordComposite(RenderPassContext& context);

		bool m_DrawnThisFrame = false;
	};
}
