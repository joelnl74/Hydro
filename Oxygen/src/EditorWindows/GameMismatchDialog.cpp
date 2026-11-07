#include "GameMismatchDialog.h"

#include <Hydro/Core/Log.h>
#include <Hydro/Utils/PlatformUtils.h>

#include <imgui.h>

namespace Hydro
{
	static constexpr const char* PopupName = "Another game's project";

	void GameMismatchDialog::Open(const std::filesystem::path& project, const GameInfo& game)
	{
		m_Project = project;
		m_Game = game;
		m_OpenRequested = true;
	}

	std::filesystem::path GameMismatchDialog::FindEditor(const RecentProjects& recent) const
	{
		std::error_code ec;
		const std::filesystem::path normal = std::filesystem::absolute(m_Project, ec).lexically_normal();
		std::filesystem::path sameGame;
		for (const RecentProject& entry : recent.GetEntries())
		{
			if (entry.Editor.empty() || std::filesystem::exists(entry.Editor, ec) == false || entry.Editor == Process::GetExecutablePath())
			{
				continue;
			}
			if (entry.Path.lexically_normal() == normal)
			{
				return entry.Editor;
			}
			if (sameGame.empty() && entry.Game.Id == m_Game.Id)
			{
				sameGame = entry.Editor;
			}
		}
		return sameGame;
	}

	void GameMismatchDialog::Launch(const std::filesystem::path& editor) const
	{
		ProcessSpecification specification;
		specification.CommandLine = "\"" + editor.string() + "\" \"" + m_Project.string() + "\"";
		specification.WorkingDirectory = editor.parent_path();
		if (Process::Launch(specification))
		{
			HY_INFO("Opening {} in {}", m_Project.filename().string(), editor.string());
		}
	}

	void GameMismatchDialog::OnImguiRender(const RecentProjects& recent)
	{
		if (m_OpenRequested)
		{
			ImGui::OpenPopup(PopupName);
			m_OpenRequested = false;
		}

		if (ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize) == false)
		{
			return;
		}

		const GameInfo& compiled = Project::GetCompiledGame();
		ImGui::Text("%s belongs to %s.", m_Project.filename().string().c_str(), m_Game.Name.c_str());
		ImGui::Text("This editor is built with %s.", compiled.IsGeneric() ? "no game (engine only)" : compiled.Name.c_str());
		ImGui::TextDisabled("It can't read that game's scripts, components and data: saving would lose them.");
		ImGui::Spacing();

		if (ImGui::Button("Open read-only"))
		{
			const std::filesystem::path project = m_Project;
			ImGui::CloseCurrentPopup();
			if (OnOpenReadOnly)
			{
				OnOpenReadOnly(project);
			}
		}
		ImGui::SameLine();
		const std::filesystem::path editor = FindEditor(recent);
		const std::string launchLabel = "Open in the " + m_Game.Name + " editor";
		if (editor.empty() == false && ImGui::Button(launchLabel.c_str()))
		{
			Launch(editor);
			ImGui::CloseCurrentPopup();
		}
		else if (editor.empty() && ImGui::Button("Find its editor..."))
		{
			const std::string file = FileDialogs::OpenFile("Oxygen (Oxygen.exe)\0*.exe\0");
			if (file.empty() == false)
			{
				Launch(file);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
