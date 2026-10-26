#include "RendererStatsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Renderer/Renderer3D.h>

#include <cstdio>

namespace Hydro
{
	void RendererStatsPanel::Draw()
	{
		// Renderer2D::Render() runs after ImGui is built, so this is the previous frame.
		Statistics stats = Renderer2D::GetStats();
		char a[32], b[32];

		// ---- Geometry ----------------------------------------------------------
		if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##geometry"))
		{
			StatsUI::Row("Draw calls", "%u", stats.DrawCalls);
			StatsUI::Row("Quads", "%u", stats.QuadCount);
			StatsUI::Row("Triangles", "%u", stats.GetTriangleCount());
			StatsUI::Row("Vertices", "%u", stats.GetTotalVertexCount());
			StatsUI::Row("Indices", "%u", stats.GetTotalIndexCount());
			ImGui::EndTable();
		}

		// ---- Meshes (Renderer3D) ----------------------------------------------
		const Renderer3DStatistics meshStats = Renderer3D::GetStats();
		if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##meshes"))
		{
			StatsUI::Row("Mesh draw calls", "%u", meshStats.MeshDraws);
			StatsUI::Row("Mesh triangles", "%u", meshStats.Triangles);
			StatsUI::Row("Mesh shadow draws", "%u (of the draw calls)", meshStats.ShadowDraws);
			StatsUI::Row("Cached shapes", "%u", meshStats.CachedMeshes);
			StatsUI::Row("Cached materials", "%u", meshStats.CachedMaterials);
			ImGui::EndTable();
		}

		// ---- Batching ----------------------------------------------------------
		if (ImGui::CollapsingHeader("Batching", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##batching"))
		{
			StatsUI::Row("Batches used", "%u / %u allocated", stats.BatchCount, stats.BatchesAllocated);
			StatsUI::Row("Quads per batch", "%u max", stats.MaxQuadsPerBatch);

			// How full the last batch is - tells you how close you are to another draw call.
			uint32_t quadsInLast = stats.BatchCount == 0 || stats.MaxQuadsPerBatch == 0 ? 0
				: stats.QuadCount - (stats.BatchCount - 1) * stats.MaxQuadsPerBatch;
			float fill = stats.MaxQuadsPerBatch ? (float)quadsInLast / stats.MaxQuadsPerBatch : 0.0f;

			char fillText[32];
			snprintf(fillText, sizeof(fillText), "%u (%.0f%%)", quadsInLast, fill * 100.0f);
			StatsUI::BarRow("Last batch fill", fill, fillText);

			StatsUI::Row("Vertex upload", "%s / frame", StatsUI::FormatBytes(stats.VertexBytesUploaded, a, sizeof(a)));
			ImGui::EndTable();
		}

		// ---- Textures ----------------------------------------------------------
		if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##textures"))
		{
			StatsUI::Row("Slots per draw", "%u (slot 0 = white)", stats.MaxTextureSlots);
			StatsUI::Row("Slots used", "%u", stats.TextureSlotsUsed);
			StatsUI::Row("Texture flushes", "%u", stats.TextureFlushes);
			ImGui::EndTable();
		}

		// ---- Forward lighting and debug lines (M6, M7) ---------------------------
		if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##lighting"))
		{
			StatsUI::Row("Forward-lit views", "%u", stats.ForwardLitViewCount);
			StatsUI::Row("Forward lights", "%u (point and spot)", stats.ForwardLightCount);
			StatsUI::Row("Lights dropped", "%u (beyond %u per view)", stats.ForwardLightsDropped, (uint32_t)MaxForwardLightsPerView);
			StatsUI::Row("Light-buffer views", "%u (%u lights, %u shadows)", stats.LitViewCount, stats.LightCount, stats.ShadowCount);
			StatsUI::Row("Debug lines", "%u", stats.DebugLineCount);
			StatsUI::Row("Sun shadow maps", "%u", stats.SunShadowViewCount);
			StatsUI::Row("Sprite shadow casters", "%u", stats.ShadowCasterQuads);
			StatsUI::Row("HDR views", "%u (scene colour + tonemap)", stats.HDRViewCount);
			StatsUI::Row("Bloom", "%u views, %u levels from %u x %u", stats.BloomViewCount, stats.BloomLevelCount, stats.BloomWidth, stats.BloomHeight);
			ImGui::EndTable();
		}

		// ---- Passes: GPU time per render-graph pass (summed over views, a couple of frames old) ----
		const std::vector<PassTiming>& passes = Renderer2D::GetPassTimings();
		if (ImGui::CollapsingHeader("Passes", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##passes"))
		{
			if (passes.empty())
			{
				StatsUI::Row("GPU time", "n/a (no timestamp support, or nothing drawn yet)");
			}
			for (const PassTiming& pass : passes)
			{
				StatsUI::Row(pass.Name.c_str(), "%.3f ms", pass.GPUMs);
			}
			ImGui::EndTable();
		}

		// ---- GPU ---------------------------------------------------------------
		if (ImGui::CollapsingHeader("GPU", ImGuiTreeNodeFlags_DefaultOpen) && StatsUI::BeginStatTable("##gpu"))
		{
			StatsUI::Row("Device", "%s", stats.GPUName);
			if (stats.GPUTimeMs >= 0.0f)
				StatsUI::Row("2D pass GPU time", "%.3f ms", stats.GPUTimeMs);
			else
				StatsUI::Row("2D pass GPU time", "n/a (no timestamp support)");
			StatsUI::Row("GPU memory", "%s used / %s free",
				StatsUI::FormatBytes(stats.GPUMemoryUsed, a, sizeof(a)),
				StatsUI::FormatBytes(stats.GPUMemoryFree, b, sizeof(b)));
			StatsUI::Row("Render target", "%u x %u", stats.ViewportWidth, stats.ViewportHeight);
			StatsUI::Row("Frames in flight", "%u (current %u)", stats.FramesInFlight, stats.FrameIndex);
			ImGui::EndTable();
		}
	}
}
