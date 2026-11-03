#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

namespace Hydro
{
	// Bloom at PostProcess, read by the tonemap: the scene colour's bright parts at half resolution, down a chain of
	// smaller levels and back up, so level 0 ends up holding the glow. Needs HDR; without it, only reports that.
	class BloomPass : public RenderPass
	{
	public:
		explicit BloomPass(bool hdr) : m_HDR(hdr) {}

		const char* GetName() const override { return "Bloom"; }
		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override;
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_HDR;
	};
}
