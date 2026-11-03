#pragma once
#include "../EditorSettings.h"

#include <Hydro/Core/Log.h>

#include <imgui/imgui.h>

#include <cstdint>
#include <deque>
#include <vector>

namespace Hydro
{
	// The "Console" window: the engine and client log, including messages from before it opened (the last
	// Log::MaxKeptMessages). What each line shows comes from Editor Settings > Console.
	class ConsoleWindow
	{
	public:
		void OnImguiRender(ConsoleSettings& settings);

		bool& IsOpen() { return m_Open; }

		// Empties this window only; the log files and terminal keep their messages.
		void Clear();

	private:
		void PullNewMessages();
		bool PassesFilter(const LogMessage& message) const;

	private:
		bool m_Open = true;

		std::deque<LogMessage> m_Messages;
		uint64_t m_LastSequence = 0;
		std::vector<LogMessage> m_Incoming; // reused every frame

		// Which levels to show, and a text filter ("-word" excludes, "a,b" matches either).
		bool m_ShowLevel[5] = { true, true, true, true, true };
		ImGuiTextFilter m_TextFilter;
	};
}
