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
			ImGui::EndMenu();
		}
	}

	void EditorMenuBar::DrawCreateMenu()
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Empty Entity"))
			{
				Entity entity = m_Context.ActiveScene->CreateEntity("Entity");
				m_Context.Selection = entity;
			}
			if (ImGui::MenuItem("Sprite"))
			{
				Entity entity = m_Context.ActiveScene->CreateEntity("Sprite");
				entity.AddComponent<TransformComponent>();
				entity.AddComponent<SpriteRendererComponent>();
				m_Context.Selection = entity;
			}
			if (ImGui::MenuItem("Camera"))
			{
				Entity entity = m_Context.ActiveScene->CreateEntity("Camera");
				entity.AddComponent<TransformComponent>();
				entity.AddComponent<CameraComponent>();
				m_Context.Selection = entity;
			}
			if (ImGui::MenuItem("Light"))
			{
				Entity entity = m_Context.ActiveScene->CreateEntity("Light");
				entity.AddComponent<TransformComponent>();
				m_Context.Selection = entity;
			}
			if (ImGui::BeginMenu("Meshes"))
			{
				if (ImGui::MenuItem("Box"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Box");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<MeshComponent>().Shape = MeshShape::Box;
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Cylinder"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Cylinder");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<MeshComponent>().Shape = MeshShape::Cylinder;
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Plane"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Plane");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<MeshComponent>().Shape = MeshShape::Plane;
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Stairs"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Stairs");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<MeshComponent>().Shape = MeshShape::Stairs;
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Ramp"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Ramp");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<MeshComponent>().Shape = MeshShape::Ramp;
					m_Context.Selection = entity;
				}


				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}
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
		m_Context.Commands.Undo();
	}

	void EditorMenuBar::Redo()
	{
		m_Context.Terrain.EndStroke(m_Context.Commands);
		m_Context.Commands.Redo();
	}
}
