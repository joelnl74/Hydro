#include "EditorMenuBar.h"
#include "Utilities/EditorChrome.h"

#include <Hydro/Core/Application.h>
#include <Hydro/Project/Project.h>
#include <Hydro/Scene/Components/Components.h>

#include <imgui/imgui.h>

#include <string>

namespace Hydro
{
	static void Run(const std::function<void()>& action)
	{
		if (action)
		{
			action();
		}
	}

	EditorMenuBar::EditorMenuBar(EditorContext& context)
		: m_Context(context)
	{
	}


	void EditorMenuBar::Draw(const std::vector<ViewWindow>& viewWindows)
	{
		if (ImGui::BeginMenuBar() == false)
		{
			return;
		}
		EditorChrome::DrawBrand(m_Context.Settings.Style.GetHighlightColor());
		DrawFileMenu();
		DrawEditMenu();
		if (ImGui::BeginMenu("View"))
		{
			for (const ViewWindow& window : viewWindows)
			{
				ImGui::MenuItem(window.Name, nullptr, window.Open);
			}
			ImGui::EndMenu();
		}
		DrawCreateMenu();
		DrawPlayControls();
		DrawSceneName();

		// The waterline: the accent while editing, the play-mode tint while playing.
		const bool tinted = m_Context.Playing && m_Context.Settings.PlayMode.Tint;
		EditorChrome::DrawMenuBarWaterline(tinted ? m_Context.Settings.PlayMode.TintColor
			: m_Context.Settings.Style.GetHighlightColor());

		ImGui::EndMenuBar();
	}

	void EditorMenuBar::DrawFileMenu()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Project..."))
			{
				Run(OnNewProject);
			}
			if (ImGui::MenuItem("Open Project..."))
			{
				Run(OnOpenProject);
			}
			if (ImGui::MenuItem("Save Project", nullptr, false, Project::GetActive() != nullptr))
			{
				Run(OnSaveProject);
			}
			if (ImGui::MenuItem("Build...", nullptr, false, Project::GetActive() != nullptr))
			{
				Run(OnBuild);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("New Scene", "Ctrl+N"))
			{
				Run(OnNewScene);
			}
			if (ImGui::MenuItem("New Empty Scene"))
			{
				Run(OnNewEmptyScene);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("A scene with only a camera, without the project's scene template");
			}
			if (ImGui::MenuItem("Open Scene..."))
			{
				Run(OnOpenScene);
			}
			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
			{
				Run(OnSaveScene);
			}
			if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
			{
				Run(OnSaveSceneAs);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit"))
			{
				Application::Get().Quit();
			}

			ImGui::EndMenu();
		}
	}

	void EditorMenuBar::DrawEditMenu()
	{
		if (ImGui::BeginMenu("Edit"))
		{
			const std::string undoName = m_Context.Commands.GetUndoName();
			const std::string redoName = m_Context.Commands.GetRedoName();
			const std::string undoLabel = undoName.empty() ? std::string("Undo") : "Undo " + undoName;
			const std::string redoLabel = redoName.empty() ? std::string("Redo") : "Redo " + redoName;
			if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, m_Context.Commands.CanUndo() && m_Context.Playing == false))
			{
				Undo();
			}
			if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, m_Context.Commands.CanRedo() && m_Context.Playing == false))
			{
				Redo();
			}
			ImGui::Separator();
			const bool canEdit = m_Context.Playing == false && (uint64_t)m_Context.GetSelectedUUID() != 0;
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, canEdit))
			{
				m_Context.DuplicateEntity(m_Context.Selection);
			}
			if (ImGui::MenuItem("Delete", "Delete", false, canEdit))
			{
				m_Context.DeleteEntity(m_Context.Selection);
			}
			ImGui::EndMenu();
		}
	}

	void EditorMenuBar::DrawCreateMenu()
	{
		if (ImGui::BeginMenu("Create") == false)
		{
			return;
		}
		// Each one an undo step that selects the new entity.
		auto create = [this](const char* name, const std::function<void(Entity&)>& setup)
		{
			SceneEditScope undo = m_Context.BeginSceneEdit(std::string("Create ") + name);
			Entity entity = m_Context.ActiveScene->CreateEntity(name);
			entity.AddComponent<TransformComponent>();
			if (setup)
			{
				setup(entity);
			}
			m_Context.Selection = entity;
			m_Context.FinishSceneEdit(undo);
		};
		auto mesh = [&](const char* name, MeshShape shape)
		{
			create(name, [shape](Entity& entity) { entity.AddComponent<MeshComponent>().Shape = shape; });
		};

		if (ImGui::MenuItem("Empty Entity"))
		{
			create("Entity", nullptr);
		}
		if (ImGui::MenuItem("Sprite"))
		{
			create("Sprite", [](Entity& entity) { entity.AddComponent<SpriteRendererComponent>(); });
		}
		if (ImGui::MenuItem("Camera"))
		{
			create("Camera", [](Entity& entity) { entity.AddComponent<CameraComponent>(); });
		}
		if (ImGui::MenuItem("Light"))
		{
			create("Light", [](Entity& entity) { entity.AddComponent<Light2DComponent>(); });
		}
		if (ImGui::BeginMenu("Meshes"))
		{
			const std::pair<const char*, MeshShape> shapes[] = { { "Box", MeshShape::Box }, { "Cylinder", MeshShape::Cylinder },
				{ "Plane", MeshShape::Plane }, { "Stairs", MeshShape::Stairs }, { "Ramp", MeshShape::Ramp } };
			for (const auto& [name, shape] : shapes)
			{
				if (ImGui::MenuItem(name))
				{
					mesh(name, shape);
				}
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}

	void EditorMenuBar::DrawPlayControls()
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float buttonSize = ImGui::GetFrameHeight() * 1.4f;
		const float totalWidth = 3.0f * buttonSize + 2.0f * style.ItemSpacing.x;
		const float centre = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
		if (centre > ImGui::GetCursorPosX())
		{
			ImGui::SetCursorPosX(centre);
		}

		const ImVec4 accent = m_Context.Settings.Style.GetHighlightColor();
		const ImVec4 playColor = m_Context.Settings.PlayMode.Tint ? m_Context.Settings.PlayMode.TintColor : accent;

		// Play turns into Stop (in the play-mode colour) while the game runs.
		const bool playing = m_Context.Playing;
		if (EditorChrome::IconButton("##PlayStop", playing ? ToolbarIcon::Stop : ToolbarIcon::Play, buttonSize, playing, playColor,
			playing ? "Stop (back to the editor scene)" : "Play (runs a copy of the scene)"))
		{
			if (playing)
			{
				Run(OnStop);
			}
			else
			{
				Run(OnPlay);
			}
		}

		ImGui::BeginDisabled(playing == false);

		const bool paused = m_Context.Paused;
		if (EditorChrome::IconButton("##Pause", paused ? ToolbarIcon::Play : ToolbarIcon::Pause, buttonSize, paused, accent,
			paused ? "Resume" : "Pause"))
		{
			m_Context.Paused = m_Context.Paused == false;
		}

		ImGui::BeginDisabled(m_Context.Paused == false);
		if (EditorChrome::IconButton("##Step", ToolbarIcon::Step, buttonSize, false, accent, "Step one frame"))
		{
			m_Context.StepFrames++;
		}
		ImGui::EndDisabled();

		ImGui::EndDisabled();
	}

	void EditorMenuBar::UpdateKeys()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (m_Context.Playing || io.WantTextInput || io.KeyCtrl == false)
		{
			return;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			if (io.KeyShift)
			{
				Redo();
			}
			else
			{
				Undo();
			}
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
		{
			Redo();
		}
	}

	void EditorMenuBar::Undo()
	{
		m_Context.Terrain.EndStroke(m_Context.Commands);
		const UUID selected = m_Context.GetSelectedUUID();
		const EditorCommand* command = m_Context.Commands.PeekUndo();
		if (m_Context.Commands.Undo())
		{
			FollowSelection(command, false, selected);
		}
	}

	void EditorMenuBar::Redo()
	{
		m_Context.Terrain.EndStroke(m_Context.Commands);
		const UUID selected = m_Context.GetSelectedUUID();
		const EditorCommand* command = m_Context.Commands.PeekRedo();
		if (m_Context.Commands.Redo())
		{
			FollowSelection(command, true, selected);
		}
	}

	void EditorMenuBar::FollowSelection(const EditorCommand* command, bool afterEdit, UUID previous)
	{
		// The step's own selection; otherwise the same entity found again (an undo may have recreated it).
		m_Context.Select(command != nullptr && command->HasSelection() ? command->GetSelection(afterEdit) : previous);
	}

	void EditorMenuBar::DrawSceneName()
	{
		if (m_Context.EditorScene == nullptr)
		{
			return;
		}
		const std::string name = m_Context.EditorScenePath.empty() ? m_Context.EditorScene->GetName()
			: m_Context.EditorScenePath.stem().u8string();
		const bool modified = m_Context.Commands.IsModified();
		const std::string label = modified ? name + " *" : name;
		const float x = ImGui::GetWindowWidth() - ImGui::CalcTextSize(label.c_str()).x - 2.0f * ImGui::GetStyle().ItemSpacing.x;
		if (x > ImGui::GetCursorPosX())
		{
			ImGui::SetCursorPosX(x);
		}
		if (modified)
		{
			ImGui::TextUnformatted(label.c_str());
		}
		else
		{
			ImGui::TextDisabled("%s", label.c_str());
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", modified ? "Unsaved changes (Ctrl+S saves the scene)" : "Saved");
		}
	}
}
