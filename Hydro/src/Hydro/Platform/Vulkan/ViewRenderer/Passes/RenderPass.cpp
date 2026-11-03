#include "hypch.h"
#include "RenderPass.h"

namespace Hydro
{
	void RenderPass::AddRecordedPass(RenderGraphBuilder& builder, const char* name, RenderStage stage, std::vector<std::string> reads,
		std::vector<std::string> writes)
	{
		// The graph owns this feature as long as its passes, so they may call back into it.
		builder.AddPass({ name, stage, std::move(reads), std::move(writes), [this](RenderPassContext& context) { Record(context); } });
	}
}
