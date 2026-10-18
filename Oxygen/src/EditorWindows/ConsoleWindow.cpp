#include "ConsoleWindow.h"

#include <imgui/imgui_internal.h>

#include <cstdio>

namespace Hydro
{
	struct LevelStyle
	{
		const char* Name;
		ImVec4 Color;
	};

	// Indexed by LogLevel.
	const LevelStyle LevelStyles[5] =
	{
		{ "Trace", { 0.60f, 0.60f, 0.60f, 1.0f } },
		{ "Info",  { 0.85f, 0.85f, 0.85f, 1.0f } },
		{ "Warn",  { 1.00f, 0.80f, 0.30f, 1.0f } },
		{ "Error", { 1.00f, 0.40f, 0.35f, 1.0f } },
		{ "Fatal", { 1.00f, 0.20f, 0.60f, 1.0f } },
	};

	void ConsoleWindow::Clear()
	{
		m_Messages.clear();
	}

	void ConsoleWindow::PullNewMessages()
	{
		m_Incoming.clear();
		m_LastSequence = Log::GetMessagesSince(m_LastSequence, m_Incoming);
		if (m_Incoming.empty())
			return;

		for (LogMessage& message : m_Incoming)
			m_Messages.push_back(std::move(message));

		while (m_Messages.size() > Log::MaxKeptMessages)
			m_Messages.pop_front();

	}

	bool ConsoleWindow::PassesFilter(const LogMessage& message) const
	{
		if (m_ShowLevel[(int)message.Level] == false)
			return false;

		if (m_TextFilter.IsActive() == false)
			return true;

		return m_TextFilter.PassFilter(message.Text.c_str()) || m_TextFilter.PassFilter(message.Source.c_str());
	}

	void ConsoleWindow::OnImguiRender()
	{
		// Keep collecting while closed, so nothing is missed (the history only keeps the newest messages).
		PullNewMessages();

		if (m_Open == false)
			return;

		// First time only (no saved layout for it yet): dock as a tab next to the Project window.
		if (ImGuiWindow* project = ImGui::FindWindowByName("Project"))
		{
			if (project->DockId != 0)
				ImGui::SetNextWindowDockID(project->DockId, ImGuiCond_FirstUseEver);
		}

		if (ImGui::Begin("Console", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		// ---- Toolbar ------------------------------------------------------------------
		if (ImGui::Button("Clear"))
			Clear();

		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &m_AutoScroll);

		// Level toggles with counts.
		uint32_t counts[5] = {};
		for (const LogMessage& message : m_Messages)
			counts[(int)message.Level]++;

		for (int level = 0; level < 5; level++)
		{
			char label[32];
			std::snprintf(label, sizeof(label), "%s (%u)", LevelStyles[level].Name, counts[level]);

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, LevelStyles[level].Color);
			ImGui::Checkbox(label, &m_ShowLevel[level]);
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();
		m_TextFilter.Draw("##Filter", -1.0f);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Filter text. \"a,b\" shows lines with a or b; \"-a\" hides lines with a.");

		ImGui::Separator();

		// ---- Messages -----------------------------------------------------------------
		ImGui::BeginChild("##ConsoleScroll", { 0.0f, 0.0f }, false, ImGuiWindowFlags_HorizontalScrollbar);

		// Only filtered lines are listed; the clipper then draws just the visible ones.
		std::vector<const LogMessage*> visible;
		visible.reserve(m_Messages.size());
		for (const LogMessage& message : m_Messages)
		{
			if (PassesFilter(message))
				visible.push_back(&message);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4.0f, 1.0f });
		ImGuiListClipper clipper;
		clipper.Begin((int)visible.size());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			{
				const LogMessage& message = *visible[i];
				const LevelStyle& style = LevelStyles[(int)message.Level];

				ImGui::PushID(i);
				ImGui::TextDisabled("%s", message.Time.c_str());
				ImGui::SameLine();
				ImGui::TextColored(style.Color, "%-5s %s:", style.Name, message.Source.c_str());
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, style.Color);
				ImGui::TextUnformatted(message.Text.c_str());
				ImGui::PopStyleColor();

				// Right-click a line to copy it.
				if (ImGui::BeginPopupContextItem("##Line"))
				{
					if (ImGui::MenuItem("Copy"))
						ImGui::SetClipboardText(message.Text.c_str());
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
		}
		clipper.End();
		ImGui::PopStyleVar();

		// Follow new messages while scrolled to the bottom; scrolling up to read stops following.
		if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}
}
