#include "RuntimeLayer.h"

#include <imgui.h>

#include <algorithm>

namespace Hydro
{
	RuntimeLayer::RuntimeLayer(const std::filesystem::path& projectPath)
		: m_ProjectPath(projectPath)
	{
	}

	void RuntimeLayer::OnAttach()
	{
		// ImGui only shows the game's picture: no saved window layout, and the keys are the game's (no
		// keyboard navigation).
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

		Window& window = Application::Get().GetWindow();
		RenderViewSpecification viewSpecification;
		viewSpecification.DebugName = "Game";
		viewSpecification.Width = std::max(1u, window.GetWidth());
		viewSpecification.Height = std::max(1u, window.GetHeight());
		m_View = CreateScope<RenderView>(viewSpecification);

		const Ref<Scene> scene = OpenProject() ? LoadStartScene() : nullptr;
		if (scene == nullptr)
		{
			// Already logged: there is nothing to play.
			Application::Get().Quit(1);
			return;
		}

		scene->OnViewportResize(m_View->GetWidth(), m_View->GetHeight());
		m_SceneRenderer = CreateScope<SceneRenderer>(scene);
		m_Scenes.Start(scene);
		HY_INFO("Playing {} from {}", scene->GetName(), m_ProjectPath.string());
	}

	void RuntimeLayer::OnDetach()
	{
		// The scenes first (their scripts' OnDestroy), then the GPU objects, then the project.
		m_Scenes.Stop();
		m_SceneRenderer.reset();
		m_View.reset();
		Project::Close();
	}

	bool RuntimeLayer::OpenProject()
	{
		std::error_code ec;
		if (std::filesystem::exists(m_ProjectPath, ec) == false)
		{
			HY_ERROR("No project at {}. A packaged game plays Project/<exe name>.hyproj next to the exe; otherwise pass --project <file.hyproj>",
				m_ProjectPath.string());
			return false;
		}

		Ref<Project> project = Project::Load(m_ProjectPath);
		if (project == nullptr)
		{
			return false; // Already logged.
		}

		Renderer::SetSettings(project->GetConfig().Renderer);
		AssetManager::OnProjectOpened(project->GetAssetDirectory());
		m_ScenesDirectory = project->GetAssetDirectory() / "Scenes";
		return true;
	}

	Ref<Scene> RuntimeLayer::LoadStartScene()
	{
		const ProjectConfig& config = Project::GetActive()->GetConfig();

		// A project file from before handles names its start scene by path.
		AssetHandle handle = config.StartScene;
		if ((uint64_t)handle == 0 && config.LegacyStartScene.empty() == false)
		{
			handle = AssetManager::ImportAsset(config.LegacyStartScene);
		}

		std::error_code ec;
		const std::filesystem::path path = (uint64_t)handle != 0 ? AssetManager::GetAbsolutePath(handle) : std::filesystem::path();
		if ((uint64_t)handle == 0 || AssetManager::IsAssetHandleValid(handle) == false || std::filesystem::exists(path, ec) == false)
		{
			HY_ERROR("The project has no start scene, or it is missing: pick one in Oxygen's Project Settings");
			return nullptr;
		}

		Ref<Scene> scene = CreateRef<Scene>(path.stem().string(), false);
		SceneSerializer serializer(scene);
		if (serializer.Deserialize(path) == false)
		{
			HY_ERROR("Could not load the start scene {}", path.string());
			return nullptr;
		}

		scene->SetName(path.stem().string());
		scene->Init();
		return scene;
	}

	void RuntimeLayer::OnUpdate(float deltaTime)
	{
		// Between ImGui frames, like the editor's full screen.
		UpdateFullscreenKey();

		if (m_Scenes.IsRunning() == false)
		{
			return;
		}

		// Scripts and engine systems.
		m_Scenes.GetActiveScene()->OnUpdateRuntime(deltaTime);

		// A script may have asked for another scene or to quit (the scene was not touched while it ran).
		const SceneTransition::Type done = m_Scenes.ProcessTransition(m_ScenesDirectory, m_View->GetWidth(), m_View->GetHeight());
		if (done == SceneTransition::Type::Quit)
		{
			HY_INFO("The game asked to quit");
			Application::Get().Quit();
			return;
		}
		if (done != SceneTransition::Type::None)
		{
			m_SceneRenderer->SetScene(m_Scenes.GetActiveScene());
		}

		// Render submission through the scene's primary camera.
		m_HasCamera = m_SceneRenderer->RenderRuntime(*m_View);
	}

	void RuntimeLayer::UpdateFullscreenKey()
	{
		// F11 toggles full screen. Escape is left to the game (menus use it).
		const bool f11 = Input::IsKeyPressed(KeyCode::F11);
		if (f11 && m_FullscreenKeyDown == false)
		{
			Window& window = Application::Get().GetWindow();
			window.SetFullscreen(window.IsFullscreen() == false);
		}
		m_FullscreenKeyDown = f11;
	}

	void RuntimeLayer::OnImGuiRender()
	{
		if (m_View == nullptr)
		{
			return;
		}

		// One undecorated window over the whole main window, holding the game's picture.
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		ImGui::Begin("##Game", nullptr, flags);
		ImGui::PopStyleVar(3);

		const ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x >= 1.0f && size.y >= 1.0f)
		{
			ResizeView((uint32_t)size.x, (uint32_t)size.y);

			const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
			ImGui::Image((ImTextureID)m_View->GetImGuiTexture(), size);
			UpdatePointer({ imagePosition.x, imagePosition.y }, { size.x, size.y });

			if (m_HasCamera == false)
			{
				ImGui::GetWindowDrawList()->AddText({ imagePosition.x + 12.0f, imagePosition.y + 12.0f }, ImGui::GetColorU32(ImGuiCol_Text),
					"No primary camera in the scene.");
			}
		}

		ImGui::End();
	}

	void RuntimeLayer::ResizeView(uint32_t width, uint32_t height)
	{
		// The game's cameras take their aspect ratio from the scene's viewport.
		m_View->SetSize(width, height);
		if (m_Scenes.IsRunning())
		{
			m_Scenes.GetActiveScene()->OnViewportResize(width, height);
		}
	}

	void RuntimeLayer::UpdatePointer(const glm::vec2& imagePosition, const glm::vec2& imageSize)
	{
		if (m_Scenes.IsRunning() == false)
		{
			return;
		}

		// The runtime UI's pointer, in the view's pixels with Y up (like the editor's Game view).
		const ImVec2 mouse = ImGui::GetMousePos();
		const float scaleX = (float)m_View->GetWidth() / imageSize.x;
		const float scaleY = (float)m_View->GetHeight() / imageSize.y;
		const glm::vec2 position{ (mouse.x - imagePosition.x) * scaleX, (imagePosition.y + imageSize.y - mouse.y) * scaleY };
		m_Scenes.GetActiveScene()->SetUIPointer(position, ImGui::IsItemHovered(), ImGui::IsMouseDown(ImGuiMouseButton_Left));
	}
}
