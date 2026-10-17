#pragma once
#include "Hydro.h"
#include "EditorWindows/HierachyWindow.h"
#include "EditorWindows/StatsWindow.h"
#include "EditorWindows/InspectorWindow.h"
#include "EditorWindows/ProjectWindow.h"
#include "EditorWindows/RenderSettingsWindow.h"
#include "EditorWindows/ProjectSettingsWindow.h"

#include <glm/glm.hpp>

namespace Hydro
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer() {};
		virtual ~EditorLayer() {};

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		// Projects (.hyproj, YAML). Opening one applies its render settings and opens its start scene.
		void NewProject();
		bool OpenProject();
		void OpenProject(const std::filesystem::path& path);
		// Copies the live render settings into the project, then writes the .hyproj.
		void SaveProject();
		// Opens the first .hyproj in EditorPaths::GetDefaultProjectDirectory(), or starts an unsaved one there.
		void OpenDefaultProject();

		// Scenes (.hyscene, YAML).
		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();
		void SetActiveScene(const Ref<Scene>& scene);

		// Prefab / scene dropped from the project window onto the viewport.
		void OnAssetDropped(const std::filesystem::path& path);

		// Viewport: the camera menu, and the image the scene is rendered into.
		void DrawViewport();
		// A new scene starts with a primary camera framing the origin.
		void CreateDefaultCamera();

		// Demo scene: animated spiral galaxy, big enough to need several batches.
		void CreateGalaxy();
		void UpdateGalaxy(float deltaTime);

	private:
		Hydro::Ref<Scene> m_Scene;
		Hydro::Scope<SceneRenderer> m_SceneRenderer;
		std::filesystem::path m_EditorScenePath; // empty until the scene is saved / opened

		// Editor Windows.
		Hydro::Ref<HierachyWindow> m_HierarchyWindow;
		Hydro::Ref<StatsWindow> m_StatsWindow;
		Hydro::Ref<InspectorWindow> m_InspectorWindow;
		Hydro::Ref<ProjectWindow> m_ProjectWindow;
		Hydro::Ref<RenderSettingsWindow> m_RenderSettingsWindow;
		Hydro::Ref<ProjectSettingsWindow> m_ProjectSettingsWindow;

		// What the viewport looks through: the editor's free camera, or the scene's primary camera
		// (the view the game will have).
		enum class ViewportCamera { Editor, Scene };
		ViewportCamera m_ViewportCamera = ViewportCamera::Editor;
		EditorCamera m_EditorCamera;
		bool m_ViewportHovered = false;
		bool m_SceneHasCamera = true; // false when the scene view found no primary camera last frame
		float m_Time = 0.0f;
	};
}

