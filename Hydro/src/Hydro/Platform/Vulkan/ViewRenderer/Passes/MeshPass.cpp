#include "hypch.h"
#include "MeshPass.h"

#include "Hydro/Renderer/Renderer3D.h"

namespace Hydro
{
	void MeshPass::Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const
	{
		if (context.Settings.Depth != DepthMode::Shared)
		{
			AddMessage(out, RenderValidationSeverity::Error, "meshes need Depth = Shared");
		}
	}

	void MeshPass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		builder.ImportResource("Depth");
		builder.ImportResource("MeshTextures");
		std::vector<std::string> writes = { "SceneColor", "Depth" };
		if (m_EditorView)
		{
			writes.push_back("EntityID");
		}
		AddRecordedPass(builder, "Meshes", RenderStage::Opaque, { "MeshTextures" }, writes);
	}

	void MeshPass::Record(RenderPassContext& context)
	{
		Renderer3D::RecordMeshes(context);
	}
}
