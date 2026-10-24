#include "ProjectSettingsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Asset/AssetManager.h>
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

		// Start scene: stored as the scene's asset handle, so renaming or moving the scene keeps it.
		std::string startScene = "(none)";
		if ((uint64_t)config.StartScene != 0)
		{
			const AssetMetadata* metadata = AssetManager::GetMetadata(config.StartScene);
			if (metadata == nullptr)
				startScene = "(unknown asset)";
			else
				startScene = metadata->Path.generic_u8string() + (metadata->IsMissing ? " (missing)" : "");
		}
		ImGui::LabelText("Start scene", "%s", startScene.c_str());

		const std::filesystem::path assetDirectory = project->GetAssetDirectory();
		// A scene saved inside the asset folder has a handle; an unsaved or outside one has none.
		const AssetHandle currentScene = currentScenePath.empty() ? AssetHandle(0) : AssetManager::FindHandle(currentScenePath);
		const bool canUseCurrent = (uint64_t)currentScene != 0;

		ImGui::BeginDisabled(canUseCurrent == false);
		if (ImGui::Button("Use current scene"))
			config.StartScene = currentScene;
		ImGui::EndDisabled();
		if (canUseCurrent == false && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Save the scene inside the project's asset folder first.");
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			config.StartScene = 0;

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
