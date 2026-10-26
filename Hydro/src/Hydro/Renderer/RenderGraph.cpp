#include "hypch.h"
#include "RenderGraph.h"

#include <algorithm>

namespace Hydro
{
	const char* RenderStageToString(RenderStage stage)
	{
		switch (stage)
		{
			case RenderStage::BeforeScene:   return "BeforeScene";
			case RenderStage::Opaque:        return "Opaque";
			case RenderStage::Translucent:   return "Translucent";
			case RenderStage::AfterLighting: return "AfterLighting";
			case RenderStage::Overlay:       return "Overlay";
			case RenderStage::Debug:         return "Debug";
			default:                         return "?";
		}
	}

	void RenderGraphBuilder::ImportResource(const std::string& name)
	{
		if (std::find(m_Imported.begin(), m_Imported.end(), name) == m_Imported.end())
		{
			m_Imported.push_back(name);
		}
	}

	void RenderGraphBuilder::AddPass(RenderPassDeclaration pass)
	{
		HY_CORE_ASSERT(pass.Stage < RenderStage::Count, "RenderGraphBuilder::AddPass: bad stage");
		HY_CORE_ASSERT((bool)pass.Execute, "RenderGraphBuilder::AddPass: pass without Execute");
		m_Passes.push_back(std::move(pass));
		m_PassFeatures.push_back(m_CurrentFeature);
	}

	std::vector<RenderValidationMessage> RenderGraph::Validate(const std::vector<Scope<RenderFeature>>& features, const RenderGraphContext& context)
	{
		std::vector<RenderValidationMessage> messages;
		for (const Scope<RenderFeature>& feature : features)
		{
			feature->Validate(context, messages);
		}
		return messages;
	}

	static bool HasError(const std::vector<RenderValidationMessage>& messages, size_t from)
	{
		for (size_t i = from; i < messages.size(); i++)
		{
			if (messages[i].Severity == RenderValidationSeverity::Error)
			{
				return true;
			}
		}
		return false;
	}

	RenderGraph::RenderGraph(std::vector<Scope<RenderFeature>> features, const RenderGraphContext& context, const std::string& viewName)
		: m_Context(context)
	{
		RenderGraphBuilder builder;

		// 1. Validate; features with an error are left out.
		for (Scope<RenderFeature>& feature : features)
		{
			const size_t first = m_Messages.size();
			feature->Validate(context, m_Messages);
			if (HasError(m_Messages, first))
			{
				continue;
			}

			// 2. Set up the rest.
			builder.m_CurrentFeature = feature->GetName();
			feature->Setup(builder);
			m_Features.push_back(std::move(feature));
		}

		// 3. Passes in stage order; within a stage, in the order they were added.
		std::vector<size_t> order(builder.m_Passes.size());
		for (size_t i = 0; i < order.size(); i++)
		{
			order[i] = i;
		}
		std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b)
		{
			return builder.m_Passes[a].Stage < builder.m_Passes[b].Stage;
		});

		// 4. Declared reads: every resource read is imported or written by an earlier pass.
		std::vector<std::string> written;
		for (size_t index : order)
		{
			const RenderPassDeclaration& pass = builder.m_Passes[index];
			for (const std::string& read : pass.Reads)
			{
				const bool imported = std::find(builder.m_Imported.begin(), builder.m_Imported.end(), read) != builder.m_Imported.end();
				const bool producedEarlier = std::find(written.begin(), written.end(), read) != written.end();
				if (imported == false && producedEarlier == false)
				{
					m_Messages.push_back({ RenderValidationSeverity::Error, builder.m_PassFeatures[index],
						"pass '" + pass.Name + "' reads '" + read + "', which no earlier pass writes and which isn't imported" });
				}
			}
			for (const std::string& write : pass.Writes)
			{
				written.push_back(write);
			}
			m_Passes.push_back(pass);
		}

		m_DepthFormat = builder.m_DepthFormat;
		m_EntityID = builder.m_EntityID;

		// Logged once per build (a pass-list rebuild happens only when the view's settings change).
		for (const RenderValidationMessage& message : m_Messages)
		{
			if (message.Severity == RenderValidationSeverity::Error)
			{
				HY_CORE_ERROR("Render graph '{}': {}: {}", viewName, message.Feature, message.Text);
			}
			else
			{
				HY_CORE_WARN("Render graph '{}': {}: {}", viewName, message.Feature, message.Text);
			}
		}
	}

	void RenderGraph::ExecuteStage(RenderStage stage, RenderPassContext& context) const
	{
		for (const RenderPassDeclaration& pass : m_Passes)
		{
			if (pass.Stage == stage)
			{
				pass.Execute(context);
			}
		}
	}

	bool RenderGraph::HasPasses(RenderStage stage) const
	{
		for (const RenderPassDeclaration& pass : m_Passes)
		{
			if (pass.Stage == stage)
			{
				return true;
			}
		}
		return false;
	}

	bool RenderGraph::HasPass(const std::string& name) const
	{
		for (const RenderPassDeclaration& pass : m_Passes)
		{
			if (pass.Name == name)
			{
				return true;
			}
		}
		return false;
	}
}
