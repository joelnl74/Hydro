#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

namespace Hydro
{
	// The HDR feature: the scene renders into an RGBA16F scene colour, and this pass (first in the output pass) writes
	// the view's sRGB colour from it with the bloom, depth of field, exposure, PBR Neutral, grading and vignette.
	class TonemapPass : public RenderPass
	{
	public:
		static constexpr const char* PassName = "Tonemap";

		TonemapPass(bool bloom, bool depthOfField) : m_Bloom(bloom), m_DepthOfField(depthOfField) {}

		const char* GetName() const override { return "HDR"; }
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_Bloom;
		bool m_DepthOfField;
	};
}
