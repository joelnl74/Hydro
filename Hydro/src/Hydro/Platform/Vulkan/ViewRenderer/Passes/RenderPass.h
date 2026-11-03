#pragma once
#include "Hydro/Renderer/RenderGraph.h"

#include <string>
#include <vector>

namespace Hydro
{
	// A render feature that records the pass it adds: Setup declares it, Record draws it every frame. One instance per
	// view's graph, so a pass may keep per-view state between frames.
	class RenderPass : public RenderFeature
	{
	public:
		virtual void Record(RenderPassContext& context) = 0;

	protected:
		// Adds a pass named `name` that calls Record.
		void AddRecordedPass(RenderGraphBuilder& builder, const char* name, RenderStage stage, std::vector<std::string> reads,
			std::vector<std::string> writes);
	};
}
