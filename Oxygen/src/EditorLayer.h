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
		Hydro::Ref<Scene> m_Scene;
		SceneRenderer* m_SceneRenderer;

		// Editor Windows.
		Hydro::Ref<HierachyWindow> m_HierarchyWindow;
	};
}

