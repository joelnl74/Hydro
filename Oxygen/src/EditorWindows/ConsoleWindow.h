#pragma once
#include "../EditorSettings.h"

#include <Hydro/Core/Log.h>

#include <imgui/imgui.h>

#include <cstdint>
#include <deque>
#include <vector>

namespace Hydro
{
	// "Console" window: the engine (Hydro) and client (APP) log, as also written to the terminal and
	// logs/*.log. Reads Log::GetMessagesSince each frame, so it shows messages from before it opened
	// too (the last Log::MaxKeptMessages). Opens docked as a tab next to the Project window.
	// What each line shows comes from the editor's ConsoleSettings (Editor Settings > Console).
	class ConsoleWindow
	{
	public:
		void OnImguiRender(ConsoleSettings& settings);

		bool& IsOpen() { return m_Open; }

		// Empties this window only; the log files and terminal are untouched.
		void Clear();

	private:
		void PullNewMessages();
		bool PassesFilter(const LogMessage& message) const;

	private:
		bool m_Open = true;

		std::deque<LogMessage> m_Messages;
		uint64_t m_LastSequence = 0;
		std::vector<LogMessage> m_Incoming; // reused every frame

		// Filters: which levels to show, and a text filter ("-word" excludes, "a,b" matches either).
		bool m_ShowLevel[5] = { true, true, true, true, true };
		ImGuiTextFilter m_TextFilter;
	};
}
