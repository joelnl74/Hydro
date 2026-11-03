#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

namespace Hydro
{
	// Ground decals (SpriteDrawMode::Decal) at Opaque, after the meshes and before the cut-out sprites. Shared depth only.
	class DecalPass : public RenderPass
	{
	public:
		explicit DecalPass(bool editorView) : m_EditorView(editorView) {}

		const char* GetName() const override { return "Sprites"; }
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_EditorView;
	};

	// Cut-out sprites, then the ones being faded, at Opaque: depth tested and written. Shared depth only.
	class CutoutSpritePass : public RenderPass
	{
	public:
		explicit CutoutSpritePass(bool editorView) : m_EditorView(editorView) {}

		const char* GetName() const override { return "Sprites"; }
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_EditorView;
	};

	// The view's translucent list in submission order (SceneRenderer sorts it): sprites, additive sprites, particles,
	// lines. Without shared depth every quad is drawn here.
	class TranslucentPass : public RenderPass
	{
	public:
		TranslucentPass(bool sharedDepth, bool editorView) : m_SharedDepth(sharedDepth), m_EditorView(editorView) {}

		const char* GetName() const override { return "Sprites"; }
		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override;
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_SharedDepth;
		bool m_EditorView;
	};

	// Quads queued between BeginOverlay and EndOverlay: on top, after the lighting and the tonemap, never depth tested.
	class OverlayPass : public RenderPass
	{
	public:
		const char* GetName() const override { return "Overlay"; }
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;
	};

	// The Editor view's debug lines (selection bounds, light reach, spot cones), last and depth tested against the scene.
	class DebugLinePass : public RenderPass
	{
	public:
		explicit DebugLinePass(bool sharedDepth) : m_SharedDepth(sharedDepth) {}

		const char* GetName() const override { return "DebugLines"; }
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_SharedDepth;
	};
}
