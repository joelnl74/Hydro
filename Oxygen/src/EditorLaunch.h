#pragma once
#include "EditorContext.h"
#include "Viewports/GameViewport.h"

#include <Hydro/Core/Application.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// Oxygen's command line (--project, --scene, --play, --frames, --capture, --validate, --exit; --help lists them),
	// so a scene can be opened, played, captured and checked unattended. See docs/TestBed.md.
	struct EditorLaunchOptions
	{
		std::filesystem::path Project;
		std::string Scene;
		bool Play = false;
		uint32_t Frames = 120;
		std::filesystem::path Capture;
		bool Validate = false;
		bool Exit = false;

		static EditorLaunchOptions Parse(const ApplicationCommandLine& commandLine);
		bool IsAutomated() const { return Capture.empty() == false || Validate || Exit; }
	};

	// Does what the command line asked for: opens its project and scene, then validates or plays; afterwards counts
	// frames, captures the Game view and quits. Opening and playing are EditorLayer's, reached through the callbacks.
	class EditorLaunch
	{
	public:
		std::function<void(const std::filesystem::path&)> OnOpenProject;
		std::function<void(const std::filesystem::path&)> OnOpenScene;
		std::function<void()> OnPlay;

		EditorLaunch(const EditorLaunchOptions& options, EditorContext& context, GameViewport& gameViewport);

		// After the startup project: the command line's project and scene, then --validate (quits) or --play.
		void Apply();
		// Every frame: counts frames for --frames, then captures (--capture) and quits (--exit).
		void Update();

		bool IsAutomated() const { return m_Options.IsAutomated(); }

	private:
		// A scene by name (in the project's Assets/Scenes) or path; empty when there is none.
		static std::filesystem::path FindScene(const std::string& nameOrPath);
		// Loads every scene of the project into a scratch scene; true when none warned.
		static bool ValidateProject();

	private:
		enum class Step : uint8_t { None, Waiting, Capturing, Done };

		EditorLaunchOptions m_Options;
		EditorContext& m_Context;
		GameViewport& m_GameViewport;
		Step m_Step = Step::None;
		uint32_t m_FramesLeft = 0;
	};
}
