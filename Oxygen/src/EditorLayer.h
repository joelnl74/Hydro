#pragma once
#include "Hydro.h"
#include "EditorWindows/HierachyWindow.h"

namespace Hydro
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer() {};
		virtual ~EditorLayer() {};

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate() override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		void NewProject();
		bool OpenProject();
		// void OpenProject(const std::filesystem::path& path);
		void SaveProject();

	private:
		Hydro::Ref<Scene> m_Scene;
		Hydro::SceneRenderer* m_SceneRenderer;

		// Editor Windows.
		Hydro::Ref<HierachyWindow> m_HierarchyWindow;
	};
}

