#include "ProjectSettingsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Project/Project.h>

#include <cstring>

namespace Hydro
{
	void ProjectSettingsWindow::OnImguiRender(const std::filesystem::path& currentScenePath)
	{
		if (m_Open == false)
			return;

		ImGui::SetNextWindowSize(ImVec2(460.0f, 300.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Project Settings", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		Ref<Project> project = Project::GetActive();
		if (project == nullptr)
		{
			ImGui::TextDisabled("No project open. Use File > New Project or Open Project.");
			ImGui::End();
			return;
		}

		ProjectConfig& config = project->GetConfig();

		// ---- General -----------------------------------------------------------
		char name[256] = {};
		strncpy(name, config.Name.c_str(), sizeof(name) - 1);
		if (ImGui::InputText("Name", name, sizeof(name)))
			config.Name = name;

		// Start scene: stored relative to the asset directory so the project folder can move.
		const std::string startScene = config.StartScene.empty() ? "(none)" : config.StartScene.generic_string();
		ImGui::LabelText("Start scene", "%s", startScene.c_str());

		const std::filesystem::path assetDirectory = project->GetAssetDirectory();
		std::error_code ec;
		const std::filesystem::path relativeScene = currentScenePath.empty() ? std::filesystem::path()
			: std::filesystem::relative(currentScenePath, assetDirectory, ec);
		const bool canUseCurrent = relativeScene.empty() == false && ec.value() == 0
			&& relativeScene.generic_string().rfind("..", 0) != 0;

		ImGui::BeginDisabled(canUseCurrent == false);
		if (ImGui::Button("Use current scene"))
			config.StartScene = relativeScene;
		ImGui::EndDisabled();
		if (canUseCurrent == false && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Save the scene inside the project's asset folder first.");
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			config.StartScene.clear();

		// ---- Locations (read-only) ---------------------------------------------
		ImGui::Spacing();
		if (StatsUI::BeginStatTable("##project_paths"))
		{
			StatsUI::Row("Project file", "%s", project->GetProjectFilePath().string().c_str());
			StatsUI::Row("Asset directory", "%s", assetDirectory.string().c_str());
			ImGui::EndTable();
		}

		ImGui::Spacing();
		if (ImGui::Button("Save project") && OnSaveProject)
			OnSaveProject();
		ImGui::SameLine();
		ImGui::TextDisabled("Also saves the render settings.");

		ImGui::End();
	}
}
