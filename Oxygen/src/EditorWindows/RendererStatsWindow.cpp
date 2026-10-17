#include "RendererStatsWindow.h"

#include <imgui/imgui.h>
#include <Hydro/Renderer/Renderer2D.h>

#include <algorithm>
#include <cstdio>
#include <cstdarg>

namespace Hydro
{
	static const char* FormatBytes(uint64_t bytes, char* buffer, size_t size)
	{
		const char* units[] = { "B", "KB", "MB", "GB" };
		double value = (double)bytes;
		int unit = 0;
		while (value >= 1024.0 && unit < 3)
		{
			value /= 1024.0;
			unit++;
		}
		snprintf(buffer, size, unit == 0 ? "%.0f %s" : "%.2f %s", value, units[unit]);
		return buffer;
	}

	// Two-column "label | value" row.
	static void StatRow(const char* label, const char* fmt, ...)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);

		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}

	void RendererStatsWindow::OnImguiRender()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Frame-time history (updated even when the window is collapsed).
		m_FrameTimes[m_FrameTimeOffset] = io.DeltaTime * 1000.0f;
		m_FrameTimeOffset = (m_FrameTimeOffset + 1) % HistorySize;

		if (!m_Open)
			return;

		if (!ImGui::Begin("Renderer Stats", &m_Open))
		{
			ImGui::End();
			return;
		}

		// Renderer2D::Render() runs after ImGui is built, so this is the previous frame.
		Statistics stats = Renderer2D::GetStats();
		char a[32], b[32];

		// ---- Performance -------------------------------------------------------
		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			float maxMs = *std::max_element(m_FrameTimes.begin(), m_FrameTimes.end());

			char overlay[48];
			snprintf(overlay, sizeof(overlay), "%.1f FPS  (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
			ImGui::PlotLines("##frametime", m_FrameTimes.data(), HistorySize, m_FrameTimeOffset,
				overlay, 0.0f, std::max(maxMs, 16.7f) * 1.2f, ImVec2(-1.0f, 60.0f));
		}

		const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;

		// ---- Geometry ----------------------------------------------------------
		if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen) &&
			ImGui::BeginTable("##geometry", 2, tableFlags))
		{
			StatRow("Draw calls", "%u", stats.DrawCalls);
			StatRow("Quads", "%u", stats.QuadCount);
			StatRow("Triangles", "%u", stats.GetTriangleCount());
			StatRow("Vertices", "%u", stats.GetTotalVertexCount());
			StatRow("Indices", "%u", stats.GetTotalIndexCount());
			ImGui::EndTable();
		}

		// ---- Batching ----------------------------------------------------------
		if (ImGui::CollapsingHeader("Batching", ImGuiTreeNodeFlags_DefaultOpen) &&
			ImGui::BeginTable("##batching", 2, tableFlags))
		{
			StatRow("Batches used", "%u / %u allocated", stats.BatchCount, stats.BatchesAllocated);
			StatRow("Quads per batch", "%u max", stats.MaxQuadsPerBatch);

			// How full the last batch is - tells you how close you are to another draw call.
			uint32_t quadsInLast = stats.BatchCount == 0 || stats.MaxQuadsPerBatch == 0 ? 0
				: stats.QuadCount - (stats.BatchCount - 1) * stats.MaxQuadsPerBatch;
			float fill = stats.MaxQuadsPerBatch ? (float)quadsInLast / stats.MaxQuadsPerBatch : 0.0f;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Last batch fill");
			ImGui::TableSetColumnIndex(1);
			char fillText[32];
			snprintf(fillText, sizeof(fillText), "%u (%.0f%%)", quadsInLast, fill * 100.0f);
			ImGui::ProgressBar(fill, ImVec2(-1.0f, 0.0f), fillText);

			StatRow("Vertex upload", "%s / frame", FormatBytes(stats.VertexBytesUploaded, a, sizeof(a)));
			ImGui::EndTable();
		}

		// ---- Memory & target ---------------------------------------------------
		if (ImGui::CollapsingHeader("GPU", ImGuiTreeNodeFlags_DefaultOpen) &&
			ImGui::BeginTable("##gpu", 2, tableFlags))
		{
			StatRow("GPU memory", "%s used / %s free",
				FormatBytes(stats.GPUMemoryUsed, a, sizeof(a)),
				FormatBytes(stats.GPUMemoryFree, b, sizeof(b)));
			StatRow("Render target", "%u x %u", stats.ViewportWidth, stats.ViewportHeight);
			StatRow("Frames in flight", "%u (current %u)", stats.FramesInFlight, stats.FrameIndex);
			ImGui::EndTable();
		}

		ImGui::End();
	}
}
