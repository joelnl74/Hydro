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
	// The fixed-stage RenderGraph v1 (3D renderer MVP plan, "Render features and presets"): features are
	// validated once, add their passes at fixed stages and declare what they read and write; the frame
	// runs the resulting pass list. Deliberately small: no dependency tracking, no automatic barriers or
	// resource scheduling, no transient resources, no pass culling, no plugin registry, no graph editor
	// (the dependency-aware graph, Roadmap RA6, comes later). Layout transitions stay hand-written in the
	// few places listed in the plan.

	// Where a pass runs. BeforeScene passes render into their own targets before the view's target is
	// begun; the others record into the view's one rendering pass, in this order.
	enum class RenderStage : uint8_t
	{
		BeforeScene = 0, // the 2D light buffer, the sun shadow map
		Opaque,          // opaque meshes, cut-out sprites
		Translucent,     // the view's one sorted translucent list
		AfterLighting,   // the 2D light composite
		Overlay,         // HUD sprites, editor outlines
		Debug,           // gizmos
		Count
	};

	const char* RenderStageToString(RenderStage stage);

	// Backend data a pass records with (the command buffer, the frame in flight, the view). Defined by the
	// backend (Platform/Vulkan/VulkanRenderViewData.h); features only pass it on.
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
		// A resource made outside the graph: the view's target images (read by ImGui, captures and picking
		// after the graph), asset textures. Never flagged as "read before written".
		void ImportResource(const std::string& name);
		void AddPass(RenderPassDeclaration pass);
		// Attachments a feature needs on the view's target.
		void RequireDepthAttachment(ImageFormat format) { m_DepthFormat = format; }
		void RequireEntityIDAttachment() { m_EntityID = true; }

	private:
		std::vector<std::string> m_Imported;
		std::vector<RenderPassDeclaration> m_Passes;
		std::vector<std::string> m_PassFeatures; // the feature that added each pass
		std::string m_CurrentFeature;
		ImageFormat m_DepthFormat = ImageFormat::None;
		bool m_EntityID = false;

		friend class RenderGraph;
	};

	// A piece of the frame. Checked and set up when the render graph is built, never per frame. One
	// instance per view's graph, so a feature may keep per-view state between its passes (the light
	// buffer knows whether it was drawn this frame).
	class RenderFeature
	{
	public:
		virtual ~RenderFeature() = default;
		virtual const char* GetName() const = 0;

		// Problems with the settings, the other features or the GPU. An error leaves this feature out;
		// a warning keeps it.
		virtual void Validate([[maybe_unused]] const RenderGraphContext& context, [[maybe_unused]] std::vector<RenderValidationMessage>& out) const {}

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
		// Validates every feature, sets up the ones without errors, then checks the declared reads. The
		// messages are kept (GetValidationMessages) and logged once.
		RenderGraph(std::vector<Scope<RenderFeature>> features, const RenderGraphContext& context, const std::string& viewName);

		RenderGraph(const RenderGraph&) = delete;
		RenderGraph& operator=(const RenderGraph&) = delete;

		// Runs the passes of one stage, in the order they were added.
		void ExecuteStage(RenderStage stage, RenderPassContext& context) const;
		bool HasPasses(RenderStage stage) const;
		// Whether a pass of that name made it into the graph (its feature passed validation).
		bool HasPass(const std::string& name) const;
		// Every pass, sorted by stage (stable), for backends that wrap each pass (debug labels, timers).
		const std::vector<RenderPassDeclaration>& GetPasses() const { return m_Passes; }

		const std::vector<RenderValidationMessage>& GetValidationMessages() const { return m_Messages; }
		const RenderGraphContext& GetContext() const { return m_Context; }
		// Attachments the view's target needs (target recreation when they change).
		ImageFormat GetDepthFormat() const { return m_DepthFormat; }
		bool NeedsEntityID() const { return m_EntityID; }

		// Validation only, for settings UI: the messages a graph built from `context` would report.
		static std::vector<RenderValidationMessage> Validate(const std::vector<Scope<RenderFeature>>& features, const RenderGraphContext& context);

	private:
		std::vector<Scope<RenderFeature>> m_Features;
		RenderGraphContext m_Context;
		std::vector<RenderPassDeclaration> m_Passes; // sorted by stage, stable
		std::vector<RenderValidationMessage> m_Messages;
		ImageFormat m_DepthFormat = ImageFormat::None;
		bool m_EntityID = false;
	};
}
