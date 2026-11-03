#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Image.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/RendererSettings.h"

#include <functional>
#include <string>
#include <vector>

namespace Hydro
{
	// A view's frame as a list of passes at fixed stages, added by render features that are validated once. Small on
	// purpose: no dependency tracking, automatic barriers or transient resources (see docs/Architecture.md).

	// Where a pass runs, in this order. Without HDR the stages after BeforeScene share the view's one rendering pass;
	// with HDR the scene pass ends after AfterLighting, PostProcess runs outside any pass, and the output pass follows.
	enum class RenderStage : uint8_t
	{
		BeforeScene = 0, // the 2D light buffer, the sun shadow map
		Opaque,          // opaque meshes, cut-out sprites
		Translucent,     // the view's one sorted translucent list
		AfterLighting,   // the 2D light composite
		PostProcess,     // HDR only, outside any pass: bloom
		Resolve,         // HDR only, first in the output pass: the tonemap (HDR scene colour -> the view's colour)
		Overlay,         // HUD sprites, editor outlines
		Debug,           // gizmos
		Count
	};

	const char* RenderStageToString(RenderStage stage);

	// The command buffer, frame in flight and view a pass records with. Defined by the backend (VulkanRenderViewData.h).
	struct RenderPassContext;

	struct RenderPassDeclaration
	{
		std::string Name;
		RenderStage Stage = RenderStage::Translucent;
		// Resources by name ("SceneColor", "Depth", "LightBuffer", "EntityID"). A read must be of an
		// imported resource or of one an earlier pass writes (checked when the graph is built).
		std::vector<std::string> Reads;
		std::vector<std::string> Writes;
		// Records the pass. It may skip itself on frame data (no lights this frame), never on settings.
		std::function<void(RenderPassContext&)> Execute;
	};

	// What a feature sees while it is validated and set up.
	struct RenderGraphContext
	{
		RenderFeatureSettings Settings;
		RendererCapabilities Capabilities;
		bool EditorView = false; // the Editor window's view (editor-only features: entity ID, debug lines)
	};

	// Collects the passes, resources and attachments of the features that passed validation.
	class RenderGraphBuilder
	{
	public:
		// A resource made outside the graph (the view's target images, asset textures): never "read before written".
		void ImportResource(const std::string& name);
		void AddPass(RenderPassDeclaration pass);
		// Attachments a feature needs on the view's target.
		void RequireDepthAttachment(ImageFormat format) { m_DepthFormat = format; }
		void RequireEntityIDAttachment() { m_EntityID = true; }
		// An HDR scene colour the scene pass draws into instead of the view's own colour.
		void RequireSceneColor(ImageFormat format) { m_SceneColorFormat = format; }
		// The depth is sampled between the scene pass and the output pass (depth of field), so it is left readable.
		void RequireSampledDepth() { m_SampledDepth = true; }

	private:
		std::vector<std::string> m_Imported;
		std::vector<RenderPassDeclaration> m_Passes;
		std::vector<std::string> m_PassFeatures; // the feature that added each pass
		std::string m_CurrentFeature;
		ImageFormat m_DepthFormat = ImageFormat::None;
		bool m_EntityID = false;
		ImageFormat m_SceneColorFormat = ImageFormat::None;
		bool m_SampledDepth = false;

		friend class RenderGraph;
	};

	// A piece of the frame, checked and set up when the graph is built, never per frame. One instance per view's
	// graph, so it may keep per-view state between its passes.
	class RenderFeature
	{
	public:
		virtual ~RenderFeature() = default;
		virtual const char* GetName() const = 0;

		// An error leaves this feature out; a warning keeps it.
		virtual void Validate([[maybe_unused]] const RenderGraphContext& context,
			[[maybe_unused]] std::vector<RenderValidationMessage>& out) const {}

		// Declares imported resources and attachments, and adds the feature's passes.
		virtual void Setup(RenderGraphBuilder& builder) = 0;

	protected:
		void AddMessage(std::vector<RenderValidationMessage>& out, RenderValidationSeverity severity, const std::string& text) const
		{
			out.push_back({ severity, GetName(), text });
		}
	};

	// The pass list of one view, built from its features. Immutable once built; a settings change builds
	// a new one (a "pass-list rebuild").
	class RenderGraph
	{
	public:
		// Sets up the features that validate without errors, then checks the declared reads. Messages are logged once.
		RenderGraph(std::vector<Scope<RenderFeature>> features, const RenderGraphContext& context, const std::string& viewName);

		RenderGraph(const RenderGraph&) = delete;
		RenderGraph& operator=(const RenderGraph&) = delete;

		// Runs the passes of one stage, in the order they were added.
		void ExecuteStage(RenderStage stage, RenderPassContext& context) const;
		bool HasPasses(RenderStage stage) const;
		// Whether a pass of that name made it into the graph (its feature passed validation).
		bool HasPass(const std::string& name) const;
		// Sorted by stage (stable), for backends that wrap each pass (debug labels, timers).
		const std::vector<RenderPassDeclaration>& GetPasses() const { return m_Passes; }

		const std::vector<RenderValidationMessage>& GetValidationMessages() const { return m_Messages; }
		const RenderGraphContext& GetContext() const { return m_Context; }
		// Attachments the view's target needs (target recreation when they change).
		ImageFormat GetDepthFormat() const { return m_DepthFormat; }
		bool NeedsEntityID() const { return m_EntityID; }
		// None: the scene draws into the view's colour (no HDR).
		ImageFormat GetSceneColorFormat() const { return m_SceneColorFormat; }
		// The depth attachment is read by a pass at PostProcess (RenderTargetSpecification::SampledDepth).
		bool NeedsSampledDepth() const { return m_SampledDepth; }

		// Validation only, for settings UI: the messages a graph built from `context` would report.
		static std::vector<RenderValidationMessage> Validate(const std::vector<Scope<RenderFeature>>& features,
			const RenderGraphContext& context);

	private:
		std::vector<Scope<RenderFeature>> m_Features;
		RenderGraphContext m_Context;
		std::vector<RenderPassDeclaration> m_Passes; // sorted by stage, stable
		std::vector<RenderValidationMessage> m_Messages;
		ImageFormat m_DepthFormat = ImageFormat::None;
		bool m_EntityID = false;
		ImageFormat m_SceneColorFormat = ImageFormat::None;
		bool m_SampledDepth = false;
	};
}
