#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

namespace Hydro
{
	struct RenderViewData;

	// The tilt-shift depth of field at PostProcess, read by the tonemap: the signed circle of confusion from the shared
	// depth, the scene colour at half size, then a gather blur. Needs HDR and shared depth.
	class DepthOfFieldPass : public RenderPass
	{
	public:
		static constexpr const char* PassName = "DepthOfField";

		explicit DepthOfFieldPass(bool hdr) : m_HDR(hdr) {}

		static void Init();
		static void Shutdown();
		// Drops the view's images (through the deletion queue): the feature or the scene's switch is off.
		static void Release(RenderViewData& view);

		const char* GetName() const override { return "DepthOfField"; }
		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override;
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_HDR;
	};
}
