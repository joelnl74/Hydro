#include "ConsoleWindow.h"

#include <imgui/imgui_internal.h>

#include <cstdio>

namespace Hydro
{
	// Indexed by LogLevel.
	static const char* const LevelNames[5] = { "Trace", "Info", "Warn", "Error", "Fatal" };

	// Trace and Info follow the theme's text colours; warnings and errors are darker on a light theme
	// so they stay readable.
	static ImVec4 GetLevelColor(int level)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const ImVec4& background = style.Colors[ImGuiCol_WindowBg];
		const bool light = 0.2126f * background.x + 0.7152f * background.y + 0.0722f * background.z > 0.5f;

		switch (level)
		{
		case 0: return style.Colors[ImGuiCol_TextDisabled];
		case 1: return style.Colors[ImGuiCol_Text];
		case 2: return light ? ImVec4(0.70f, 0.45f, 0.00f, 1.0f) : ImVec4(1.00f, 0.80f, 0.30f, 1.0f);
		case 3: return light ? ImVec4(0.80f, 0.15f, 0.12f, 1.0f) : ImVec4(1.00f, 0.42f, 0.38f, 1.0f);
		default: return light ? ImVec4(0.75f, 0.05f, 0.45f, 1.0f) : ImVec4(1.00f, 0.25f, 0.62f, 1.0f);
		}
	}

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

	void ConsoleWindow::OnImguiRender(ConsoleSettings& settings)
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
		ImGui::Checkbox("Auto-scroll", &settings.AutoScroll);

		// Level toggles with counts.
		uint32_t counts[5] = {};
		for (const LogMessage& message : m_Messages)
			counts[(int)message.Level]++;

		for (int level = 0; level < 5; level++)
		{
			char label[32];
			std::snprintf(label, sizeof(label), "%s (%u)", LevelNames[level], counts[level]);

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(level));
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
				const ImVec4 color = GetLevelColor((int)message.Level);
				const char* levelName = LevelNames[(int)message.Level];

				ImGui::PushID(i);
				if (settings.ShowTimestamps)
				{
					ImGui::TextDisabled("%s", message.Time.c_str());
					ImGui::SameLine();
				}
				if (settings.ShowSource)
					ImGui::TextColored(color, "%-5s %s:", levelName, message.Source.c_str());
				else
					ImGui::TextColored(color, "%-5s", levelName);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, color);
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
		if (settings.AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}
}
