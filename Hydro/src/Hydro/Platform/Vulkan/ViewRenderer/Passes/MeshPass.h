#pragma once
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/RenderPass.h"

namespace Hydro
{
	// Meshes and terrain surfaces (Renderer3D) at Opaque, before the sprites, so decals and sprites test against them.
	// Needs shared depth.
	class MeshPass : public RenderPass
	{
	public:
		explicit MeshPass(bool editorView) : m_EditorView(editorView) {}

		const char* GetName() const override { return "Meshes"; }
		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override;
		void Setup(RenderGraphBuilder& builder) override;
		void Record(RenderPassContext& context) override;

	private:
		bool m_EditorView;
	};
}
