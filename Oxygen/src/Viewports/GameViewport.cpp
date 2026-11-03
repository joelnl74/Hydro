#include "GameViewport.h"
#include "CaptureControls.h"
#include "EditorPaths.h"
#include "Utilities/EditorChrome.h"

#include <Hydro/Core/Application.h>
#include <Hydro/Core/Input.h>

#include <algorithm>

namespace Hydro
{
	GameViewport::GameViewport(EditorContext& context)
		: m_Context(context)
	{
	}

	void GameViewport::Init()
	{
		RenderViewSpecification specification;
		specification.DebugName = "Game";
		m_View = CreateScope<RenderView>(specification);
	}

	void GameViewport::Shutdown()
	{
		m_View.reset();
	}

	void GameViewport::OnUpdate(float deltaTime, SceneRenderer& renderer)
	{
		// The window goes in or out of full screen here, between ImGui frames (SetFullscreen asks).
		if (m_PendingWindowFullscreen >= 0)
		{
			Window& window = Application::Get().GetWindow();
			window.SetFullscreen(m_PendingWindowFullscreen == 1);
			m_WindowFullscreenByGame = m_PendingWindowFullscreen == 1 && window.IsFullscreen();
			m_PendingWindowFullscreen = -1;
		}
		m_FullscreenHintTime = std::max(0.0f, m_FullscreenHintTime - deltaTime);

		if (m_Visible || m_Fullscreen)
		{
			m_SceneHasCamera = renderer.RenderRuntime(*m_View);
		}
	}

	void GameViewport::DrawStatus(const ImVec2& textPosition)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (m_SceneHasCamera == false)
		{
			drawList->AddText(textPosition, IM_COL32(255, 200, 80, 255),
				"No primary camera in the scene. Add a Camera component and tick Primary.");
		}
		else if (m_Context.Playing && m_Context.Paused)
		{
			drawList->AddText(textPosition, IM_COL32(255, 200, 80, 255), "Paused");
		}
	}


	void GameViewport::OnImGuiRender()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		m_Visible = ImGui::Begin("Game", nullptr, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::MenuItem("Full screen", "F11", m_Fullscreen))
			{
				SetFullscreen(m_Fullscreen == false);
			}
			if (ImGui::BeginMenu("Options"))
			{
				bool changed = ImGui::MenuItem("Full screen covers the monitor", nullptr,
					&m_Context.Settings.PlayMode.FullscreenCoversMonitor);
				changed |= ImGui::MenuItem("Leave full screen on Stop", nullptr, &m_Context.Settings.PlayMode.LeaveFullscreenOnStop);
				if (changed)
				{
					EditorSettingsSerializer::Save(m_Context.Settings, EditorPaths::GetEditorSettingsPath());
				}
				ImGui::EndMenu();
			}
			CaptureControls::DrawMenu(m_Context.Captures);
			ImGui::EndMenuBar();
		}

		// The pointer is off the game until the Game view's image says otherwise (UpdatePointer).
		if (m_Context.ActiveScene != nullptr)
		{
			m_Context.ActiveScene->SetUIPointer(m_Context.ActiveScene->GetUIPointer().Position, false,
				ImGui::IsMouseDown(ImGuiMouseButton_Left));
		}

		if (m_Visible && m_Fullscreen)
		{
			ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 8.0f, ImGui::GetCursorPosY() + 8.0f));
			ImGui::TextDisabled("The game is in full screen. F11 or Escape to leave.");
		}
		else if (m_Visible)
		{
			const ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.x >= 1.0f && size.y >= 1.0f)
			{
				Resize((uint32_t)size.x, (uint32_t)size.y);
			}

			const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
			ImGui::Image((ImTextureID)m_View->GetImGuiTexture(), size);
			UpdatePointer(imagePosition, size);
			if (m_Context.Playing && m_Context.Settings.PlayMode.Tint)
			{
				EditorChrome::OutlineLastItem(m_Context.Settings.PlayMode.TintColor, 2.0f);
			}
			m_Context.AcceptAssetDrop();

			DrawStatus({ imagePosition.x + 8.0f, imagePosition.y + 8.0f });
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void GameViewport::UpdatePointer(const ImVec2& imagePosition, const ImVec2& imageSize)
	{
		if (m_Context.ActiveScene == nullptr || imageSize.x < 1.0f || imageSize.y < 1.0f)
		{
			return;
		}

		// Scale anyway: the view's size can differ from the image's for a frame after a resize.
		const ImVec2 mouse = ImGui::GetMousePos();
		const float scaleX = (float)m_View->GetWidth() / imageSize.x;
		const float scaleY = (float)m_View->GetHeight() / imageSize.y;
		const glm::vec2 position{ (mouse.x - imagePosition.x) * scaleX, (imagePosition.y + imageSize.y - mouse.y) * scaleY };
		const bool inside = ImGui::IsItemHovered();
		m_Context.ActiveScene->SetUIPointer(position, inside, ImGui::IsMouseDown(ImGuiMouseButton_Left));
	}

	void GameViewport::SetFullscreen(bool fullscreen)
	{
		if (fullscreen == m_Fullscreen)
		{
			return;
		}

		m_Fullscreen = fullscreen;
		if (fullscreen)
		{
			m_FocusFullscreen = true;
			m_FullscreenHintTime = 2.5f;
			// Only when the window isn't full screen already, so leaving gives back what the user had.
			if (m_Context.Settings.PlayMode.FullscreenCoversMonitor && Application::Get().GetWindow().IsFullscreen() == false)
			{
				m_PendingWindowFullscreen = 1;
			}
		}
		else if (m_WindowFullscreenByGame || m_PendingWindowFullscreen == 1)
		{
			m_PendingWindowFullscreen = 0;
		}
	}

	void GameViewport::UpdateKeys()
	{
		// Polled like the editor's other keys; F11 toggles, Escape only leaves.
		const bool f11 = Input::IsKeyPressed(KeyCode::F11);
		const bool escape = Input::IsKeyPressed(KeyCode::Escape);
		if (f11 && m_FullscreenKeyDown == false)
		{
			SetFullscreen(m_Fullscreen == false);
		}
		else if (escape && m_EscapeKeyDown == false && m_Fullscreen)
		{
			SetFullscreen(false);
		}
		m_FullscreenKeyDown = f11;
		m_EscapeKeyDown = escape;
	}

	void GameViewport::DrawFullscreen()
	{
		// One undecorated window over the main viewport, in front of the dockspace and every docked window.
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		if (m_FocusFullscreen)
		{
			ImGui::SetNextWindowFocus();
			m_FocusFullscreen = false;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::Begin("##GameFullscreen", nullptr, flags);
		ImGui::PopStyleVar(3);

		const ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x >= 1.0f && size.y >= 1.0f)
		{
			Resize((uint32_t)size.x, (uint32_t)size.y);
		}

		const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)m_View->GetImGuiTexture(), size);
		UpdatePointer(imagePosition, size);

		DrawStatus({ imagePosition.x + 12.0f, imagePosition.y + 12.0f });
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// How to get out, for a moment after going in.
		if (m_FullscreenHintTime > 0.0f)
		{
			const float alpha = std::min(1.0f, m_FullscreenHintTime / 0.5f);
			const char* hint = "Full screen: F11 or Escape to leave";
			const ImVec2 textSize = ImGui::CalcTextSize(hint);
			const ImVec2 position = { imagePosition.x + 0.5f * (size.x - textSize.x), imagePosition.y + 24.0f };
			drawList->AddRectFilled({ position.x - 12.0f, position.y - 6.0f },
				{ position.x + textSize.x + 12.0f, position.y + textSize.y + 6.0f },
				IM_COL32(0, 0, 0, (int)(160.0f * alpha)), 6.0f);
			drawList->AddText(position, IM_COL32(255, 255, 255, (int)(255.0f * alpha)), hint);
		}

		ImGui::End();
	}

	void GameViewport::Resize(uint32_t width, uint32_t height)
	{
		if (m_Context.Captures.Game)
		{
			if (m_Context.Captures.FixedSize)
			{
				width = CaptureRequests::FixedWidth;
				height = CaptureRequests::FixedHeight;
			}
			m_View->RequestCapture(m_Context.Captures.GamePath.empty()
				? CaptureControls::MakePath(m_Context.ActiveScene.get(), "Game") : m_Context.Captures.GamePath);
			m_Context.Captures.GamePath.clear();
			m_Context.Captures.Game = false;
		}

		// The game's cameras take their aspect ratio from the scene's viewport.
		m_View->SetSize(width, height);
		m_Context.ActiveScene->OnViewportResize(width, height);
	}
}
