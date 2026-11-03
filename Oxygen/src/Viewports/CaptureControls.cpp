#include "CaptureControls.h"
#include "EditorPaths.h"

#include <Hydro/Core/Input.h>
#include <imgui/imgui.h>

#include <chrono>
#include <cstdio>
#include <ctime>

namespace Hydro
{
	void CaptureControls::DrawMenu(CaptureRequests& requests)
	{
		if (ImGui::BeginMenu("Capture"))
		{
			if (ImGui::MenuItem("Capture Game View", "F9"))
			{
				requests.Game = true;
			}
			if (ImGui::MenuItem("Capture Editor View", "Shift+F9"))
			{
				requests.Editor = true;
			}

			ImGui::Separator();
			ImGui::MenuItem("Fixed Size (1280 x 720)", nullptr, &requests.FixedSize);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Render the captured frame at 1280 x 720, whatever the window's size,\nso captures can be compared. Pause first so the scene stands still.");
			}

			ImGui::Separator();
			ImGui::TextDisabled("Saved in <project>/.hydro/captures");
			ImGui::EndMenu();
		}
	}

	void CaptureControls::UpdateKeys(CaptureRequests& requests)
	{
		// Not F12: Windows reserves it for debuggers (UserDebuggerHotKey).
		const bool captureKey = Input::IsKeyPressed(KeyCode::F9);
		if (captureKey && requests.KeyDown == false && ImGui::GetIO().WantTextInput == false)
		{
			const bool shift = Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::RightShift);
			if (shift)
			{
				requests.Editor = true;
			}
			else
			{
				requests.Game = true;
			}
		}
		requests.KeyDown = captureKey;
	}

	std::filesystem::path CaptureControls::MakePath(const Scene* scene, const std::string& viewName)
	{
		const std::string sceneName = scene != nullptr && scene->GetName().empty() == false ? scene->GetName() : std::string("Untitled");

		const auto now = std::chrono::system_clock::now();
		const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
		const long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

		std::tm local = {};
#if defined(_WIN32)
		localtime_s(&local, &seconds);
#else
		localtime_r(&seconds, &local);
#endif
		char stamp[32] = {};
		std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
		char suffix[8] = {};
		std::snprintf(suffix, sizeof(suffix), "-%03lld", milliseconds);

		const std::string fileName = EditorPaths::ToFileName(sceneName) + "_" + viewName + "_" + stamp + suffix + ".png";
		return EditorPaths::GetCapturesDirectory() / fileName;
	}
}
