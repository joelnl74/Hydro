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
