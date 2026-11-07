#include "ProjectHubWindow.h"

#include <Hydro/Utils/PlatformUtils.h>

#include <imgui.h>

#include <cstdio>

namespace Hydro
{
	static void CopyToBuffer(char* buffer, size_t size, const std::string& text)
	{
		std::snprintf(buffer, size, "%s", text.c_str());
	}

	void ProjectHubWindow::Open(bool newProject)
	{
		m_Open = true;
		m_ShowNewProject = newProject;
	}

	void ProjectHubWindow::OnImguiRender(RecentProjects& recent, bool projectOpen)
	{
		if (projectOpen && m_Open == false)
		{
			return;
		}

		// Over the whole editor window, like a start page.
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 24.0f));
		ImGui::Begin("Project Hub", nullptr, flags);
		ImGui::PopStyleVar();

		ImGui::TextUnformatted("Oxygen projects");
		const GameInfo& game = Project::GetCompiledGame();
		ImGui::TextDisabled("This editor's game: %s", game.IsGeneric() ? "none (engine only)" : game.Name.c_str());
		ImGui::Spacing();

		if (ImGui::Button("New Project..."))
		{
			m_ShowNewProject = m_ShowNewProject == false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Open...") && OnBrowseProject)
		{
			OnBrowseProject();
		}
		if (projectOpen)
		{
			ImGui::SameLine();
			if (ImGui::Button("Back to the project"))
			{
				m_Open = false;
			}
		}
		ImGui::Separator();

		if (m_ShowNewProject)
		{
			DrawNewProject(recent);
			ImGui::Separator();
		}

		if (recent.GetEntries().empty())
		{
			ImGui::TextDisabled("No recent projects. Make a new one or open a .hyproj.");
		}
		else
		{
			DrawGroup(recent, ProjectCompatibility::Compatible, "This game");
			DrawGroup(recent, ProjectCompatibility::Generic, "Generic (no game code yet)");
			DrawGroup(recent, ProjectCompatibility::OtherGame, "Other games");
		}

		ImGui::End();

		if (m_PendingRemove.empty() == false)
		{
			recent.Remove(m_PendingRemove);
			m_PendingRemove.clear();
			if (OnRecentChanged)
			{
				OnRecentChanged();
			}
		}
		if (m_PendingOpen.empty() == false)
		{
			const std::filesystem::path path = m_PendingOpen;
			m_PendingOpen.clear();
			if (OnOpenProject)
			{
				OnOpenProject(path);
			}
		}
	}

	void ProjectHubWindow::DrawNewProject(const RecentProjects& recent)
	{
		// Defaults to the folder the last project's folder is in.
		if (m_NewLocation[0] == '\0' && recent.GetEntries().empty() == false)
		{
			CopyToBuffer(m_NewLocation.data(), m_NewLocation.size(),
				recent.GetEntries().front().Path.parent_path().parent_path().string());
		}

		ImGui::TextUnformatted("New project");
		ImGui::SetNextItemWidth(320.0f);
		ImGui::InputText("Name", m_NewName.data(), m_NewName.size());
		ImGui::SetNextItemWidth(320.0f);
		ImGui::InputText("Location", m_NewLocation.data(), m_NewLocation.size());
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			const std::string folder = FileDialogs::OpenDirectory();
			if (folder.empty() == false)
			{
				CopyToBuffer(m_NewLocation.data(), m_NewLocation.size(), folder);
			}
		}

		const std::string name = m_NewName.data();
		const std::filesystem::path location = m_NewLocation.data();
		std::error_code ec;
		const bool exists = name.empty() == false && std::filesystem::exists(location / name, ec);
		const bool valid = name.empty() == false && location.empty() == false && exists == false;
		if (location.empty() == false && name.empty() == false)
		{
			ImGui::TextDisabled("%s", (location / name / (name + Project::FileExtension)).string().c_str());
		}
		if (exists)
		{
			ImGui::TextDisabled("That folder already exists: pick another name or location.");
		}

		ImGui::BeginDisabled(valid == false);
		if (ImGui::Button("Create") && OnNewProject)
		{
			OnNewProject(name, location);
			m_ShowNewProject = false;
			m_NewName.fill('\0');
		}
		ImGui::EndDisabled();
	}

	void ProjectHubWindow::DrawGroup(const RecentProjects& recent, ProjectCompatibility compatibility, const char* title)
	{
		bool header = false;
		for (const RecentProject& entry : recent.GetEntries())
		{
			if (entry.GetCompatibility() != compatibility)
			{
				continue;
			}
			if (header == false)
			{
				ImGui::Spacing();
				ImGui::TextDisabled("%s", title);
				header = true;
			}
			DrawEntry(entry);
		}
	}

	void ProjectHubWindow::DrawEntry(const RecentProject& entry)
	{
		std::error_code ec;
		const bool missing = std::filesystem::exists(entry.Path, ec) == false;
		const bool otherGame = entry.GetCompatibility() == ProjectCompatibility::OtherGame;
		const std::string id = entry.Path.generic_string();

		ImGui::PushID(id.c_str());
		ImGui::BeginDisabled(missing || otherGame);
		const std::string label = entry.Name.empty() ? entry.Path.stem().string() : entry.Name;
		if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, ImVec2(260.0f, 0.0f)))
		{
			m_PendingOpen = entry.Path;
		}
		ImGui::EndDisabled();

		ImGui::SameLine(300.0f);
		if (missing)
		{
			ImGui::TextDisabled("missing: %s", entry.Path.string().c_str());
		}
		else if (otherGame)
		{
			ImGui::TextDisabled("Requires the %s editor   %s", entry.Game.Name.c_str(), entry.Path.string().c_str());
		}
		else
		{
			ImGui::TextDisabled("%s   %s", entry.LastOpened.c_str(), entry.Path.string().c_str());
		}

		if (otherGame && missing == false && entry.Editor.empty() == false && std::filesystem::exists(entry.Editor, ec))
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Open in its editor"))
			{
				ProcessSpecification specification;
				specification.CommandLine = "\"" + entry.Editor.string() + "\" \"" + entry.Path.string() + "\"";
				specification.WorkingDirectory = entry.Editor.parent_path();
				Process::Launch(specification);
			}
		}
		if (missing)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Locate..."))
			{
				const std::string file = FileDialogs::OpenFile("Hydro Project (*.hyproj)\0*.hyproj\0");
				if (file.empty() == false)
				{
					m_PendingRemove = entry.Path;
					m_PendingOpen = file;
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove"))
			{
				m_PendingRemove = entry.Path;
			}
		}
		ImGui::PopID();
	}
}
