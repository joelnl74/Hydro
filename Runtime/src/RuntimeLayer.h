#pragma once
#include "Hydro.h"

#include <filesystem>

namespace Hydro
{
	// Plays a project without the editor: opens it, runs its start scene on a SceneStack (scripts switch
	// scenes and quit through SceneTransition) and shows the game over the whole window. V1 shows the
	// game's RenderView with one full-window ImGui::Image, the same path as the editor's Game view;
	// presenting straight to the swapchain is a later optimisation.
	class RuntimeLayer : public Layer
	{
	public:
		explicit RuntimeLayer(const std::filesystem::path& projectPath);

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnImGuiRender() override;

	private:
		// Opens m_ProjectPath and applies its settings; false (logged) when it can't.
		bool OpenProject();
		// The project's start scene, loaded and Init'ed; null (logged) when it has none or it can't be loaded.
		Ref<Scene> LoadStartScene();
		void UpdateFullscreenKey();
		void ResizeView(uint32_t width, uint32_t height);
		void UpdatePointer(const glm::vec2& imagePosition, const glm::vec2& imageSize);

	private:
		std::filesystem::path m_ProjectPath;
		std::filesystem::path m_ScenesDirectory;

		SceneStack m_Scenes;
		Scope<RenderView> m_View;
		Scope<SceneRenderer> m_SceneRenderer;

		bool m_HasCamera = true;
		bool m_FullscreenKeyDown = false;
	};
}
