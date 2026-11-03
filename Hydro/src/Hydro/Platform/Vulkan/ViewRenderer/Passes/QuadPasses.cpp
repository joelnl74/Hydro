#include "hypch.h"
#include "QuadPasses.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/QuadBatcher.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"

namespace Hydro
{
	// Sprites write the entity ID in the Editor view.
	static std::vector<std::string> GetSpriteWrites(bool editorView)
	{
		std::vector<std::string> writes = { "SceneColor" };
		if (editorView)
		{
			writes.push_back("EntityID");
		}
		return writes;
	}

	void DecalPass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		builder.ImportResource("SpriteTextures");
		builder.ImportResource("Depth");
		AddRecordedPass(builder, "Decals", RenderStage::Opaque, { "SpriteTextures", "Depth" }, GetSpriteWrites(m_EditorView));
	}

	void DecalPass::Record(RenderPassContext& context)
	{
		QuadBatcher::Record(context.CommandBuffer, context.View, context.Frame, context.View.DecalDraws, SpriteDrawMode::Decal, false);
	}

	void CutoutSpritePass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		builder.ImportResource("SpriteTextures");
		builder.ImportResource("Depth");
		std::vector<std::string> writes = GetSpriteWrites(m_EditorView);
		writes.push_back("Depth");
		AddRecordedPass(builder, "CutoutSprites", RenderStage::Opaque, { "SpriteTextures", "Depth" }, writes);
	}

	void CutoutSpritePass::Record(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		QuadBatcher::Record(context.CommandBuffer, view, context.Frame, view.CutoutDraws, SpriteDrawMode::Cutout, false);
		QuadBatcher::Record(context.CommandBuffer, view, context.Frame, view.FadedCutoutDraws, SpriteDrawMode::FadedCutout, false);
	}

	void TranslucentPass::Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const
	{
		if (context.Settings.DefaultSpriteAlpha == SpriteAlphaDefault::Cutout && context.Settings.Depth != DepthMode::Shared)
		{
			AddMessage(out, RenderValidationSeverity::Warning, "cut-out sprites need Depth = Shared; sprites are drawn translucent");
		}
	}

	void TranslucentPass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		builder.ImportResource("SpriteTextures");
		std::vector<std::string> reads = { "SpriteTextures" };
		if (m_SharedDepth)
		{
			builder.ImportResource("Depth");
			reads.push_back("Depth");
		}
		AddRecordedPass(builder, "Sprites", RenderStage::Translucent, reads, GetSpriteWrites(m_EditorView));
	}

	void TranslucentPass::Record(RenderPassContext& context)
	{
		QuadBatcher::Record(context.CommandBuffer, context.View, context.Frame, context.View.Draws, SpriteDrawMode::Translucent, false);
	}

	void OverlayPass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		builder.ImportResource("SpriteTextures");
		AddRecordedPass(builder, "Overlay", RenderStage::Overlay, { "SpriteTextures" }, { "SceneColor" });
	}

	void OverlayPass::Record(RenderPassContext& context)
	{
		QuadBatcher::Record(context.CommandBuffer, context.View, context.Frame, context.View.OverlayDraws, SpriteDrawMode::Translucent,
			true);
	}

	void DebugLinePass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		std::vector<std::string> reads = {};
		if (m_SharedDepth)
		{
			builder.ImportResource("Depth");
			reads.push_back("Depth");
		}
		AddRecordedPass(builder, "DebugLines", RenderStage::Debug, reads, { "SceneColor" });
	}

	void DebugLinePass::Record(RenderPassContext& context)
	{
		QuadBatcher::Record(context.CommandBuffer, context.View, context.Frame, context.View.DebugDraws, SpriteDrawMode::Translucent, false,
			true);
	}
}
