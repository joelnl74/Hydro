#include "ProjectSettingsWindow.h"
#include "StatsUI.h"

#include <imgui/imgui.h>
#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Project/Project.h>

#include <cstring>

namespace Hydro
{
	// One scene setting: the scene's path (or why there is none), "Use current scene" and "Clear".
	static void DrawSceneSetting(const char* label, const char* tooltip, AssetHandle& scene, AssetHandle currentScene)
	{
		ImGui::PushID(label);
		std::string text = "(none)";
		if ((uint64_t)scene != 0)
		{
			const AssetMetadata* metadata = AssetManager::GetMetadata(scene);
			if (metadata == nullptr)
			{
				text = "(unknown asset)";
			}
			else
			{
				text = metadata->Path.generic_u8string() + (metadata->IsMissing ? " (missing)" : "");
			}
		}
		ImGui::LabelText(label, "%s", text.c_str());
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", tooltip);
		}

		const bool canUseCurrent = (uint64_t)currentScene != 0;
		ImGui::BeginDisabled(canUseCurrent == false);
		if (ImGui::Button("Use current scene"))
		{
			scene = currentScene;
		}
		ImGui::EndDisabled();
		if (canUseCurrent == false && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Save the scene inside the project's asset folder first.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			scene = 0;
		}
		ImGui::PopID();
	}

	void ProjectSettingsWindow::OnImguiRender(const std::filesystem::path& currentScenePath)
	{
		if (m_Open == false)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(460.0f, 360.0f), ImGuiCond_FirstUseEver);
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
		{
			config.Name = name;
		}
		ImGui::Spacing();

		// A scene saved inside the asset folder has a handle; an unsaved or outside one has none.
		const AssetHandle currentScene = currentScenePath.empty() ? AssetHandle(0) : AssetManager::FindHandle(currentScenePath);

		// Both stored as the scene's asset handle, so renaming or moving the scene keeps them.
		DrawSceneSetting("Start scene", "The scene the game and the editor open first.", config.StartScene, currentScene);
		DrawSceneSetting("Scene template", "New Scene starts as a copy of this scene (its render settings, look, camera, light and director). New Empty Scene ignores it.", config.SceneTemplate, currentScene);

		// ---- Locations (read-only) ---------------------------------------------
		ImGui::Spacing();
		if (StatsUI::BeginStatTable("##project_paths"))
		{
			StatsUI::Row("Project file", "%s", project->GetProjectFilePath().string().c_str());
			StatsUI::Row("Asset directory", "%s", project->GetAssetDirectory().string().c_str());
			ImGui::EndTable();
		}

		ImGui::Spacing();
		if (ImGui::Button("Save project") && OnSaveProject)
		{
			OnSaveProject();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Also saves the render settings.");

		ImGui::End();
	}
}
