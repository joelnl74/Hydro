#pragma once
#include <imgui/imgui.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>

// Small helpers shared by the stats panels so every tab looks the same.
namespace Hydro::StatsUI
{
	inline const char* FormatBytes(uint64_t bytes, char* buffer, size_t size)
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

	// Two-column "label | value" table. Call ImGui::EndTable() when it returns true.
	inline bool BeginStatTable(const char* id)
	{
		if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			return false;
		ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.45f);
		ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
		return true;
	}

	inline void Row(const char* label, const char* fmt, ...)
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

	// Row whose value is a progress bar (fraction 0..1) with text on it.
	inline void BarRow(const char* label, float fraction, const char* overlay)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		ImGui::ProgressBar(fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction), ImVec2(-1.0f, 0.0f), overlay);
	}
}
