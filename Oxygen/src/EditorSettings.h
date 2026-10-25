#pragma once
#include "EditorWindows/EditorStyle.h"

#include <Hydro/Renderer/EditorCamera.h>

#include <filesystem>
#include <map>
#include <string>

namespace Hydro
{
	// While the game runs, the editor's windows take on this colour, so it's obvious that changes
	// made now are thrown away on Stop.
	struct PlayModeSettings
	{
		bool Tint = true;
		ImVec4 TintColor = { 1.00f, 0.55f, 0.24f, 1.0f }; // ember
		float TintStrength = 0.12f;                       // 0 = none, 1 = the tint colour itself
		// The Game window's full screen mode (F11): also make the editor's window cover the whole monitor
		// (otherwise the game only fills the editor's window), and leave it when Play stops.
		bool FullscreenCoversMonitor = true;
		bool LeaveFullscreenOnStop = true;
	};

	struct ConsoleSettings
	{
		bool AutoScroll = true;
		bool ShowTimestamps = true;
		bool ShowSource = true; // "Hydro" / "APP"
	};

	struct EditorStartupSettings
	{
		bool ReopenLastProject = true;
		bool ReopenLastScene = true;
	};

	// Saves the open scene every IntervalMinutes while editing (never while playing, and only once
	// the scene has a file).
	struct AutosaveSettings
	{
		bool Enabled = false;
		float IntervalMinutes = 5.0f;
	};

	// What the editor had open last time. Written by the editor (on close, when a project or scene
	// is opened or saved, and when a window is opened or closed), not edited by hand.
	struct EditorSession
	{
		std::filesystem::path LastProject;
		std::filesystem::path LastScene;
		// Open or closed, per View menu window, by its menu name ("Console", "Stats", ...). A window
		// that isn't listed keeps its own default, so new windows need no entry here.
		std::map<std::string, bool> OpenWindows;
	};

	// The Oxygen editor's own settings: how the editor looks and behaves, not the game or the engine.
	// Kept per user in EditorSettings.yaml next to imgui.ini (EditorPaths::GetEditorSettingsPath),
	// not in the project. Edited live in the Editor Settings window; Save there writes them.
	struct EditorSettings
	{
		EditorStyle Style = EditorStyle::FromTheme(EditorTheme::Hydro);
		// Pixel size the UI font is loaded at. The font atlas is built at startup, so a change applies
		// after a restart (Style.FontScale applies at once, but scaled text is softer).
		float FontSize = 17.0f;

		PlayModeSettings PlayMode;

		EditorCameraSettings Camera;
		bool CameraStartsIn2D = true;

		ConsoleSettings Console;
		EditorStartupSettings Startup;
		AutosaveSettings Autosave;

		EditorSession Session;
	};

	// Reads and writes EditorSettings as YAML. Missing keys keep their defaults, so the file still
	// loads after a setting is added.
	class EditorSettingsSerializer
	{
	public:
		// Version 2: the Hydro/Abyss/Glacier themes and the accent colour. Files from version 1 keep
		// their theme and font scale; their colours and sizes are rebuilt from the new theme.
		static constexpr int Version = 2;

		static bool Save(const EditorSettings& settings, const std::filesystem::path& path);
		// Returns false (settings unchanged) when the file is missing or unreadable.
		static bool Load(const std::filesystem::path& path, EditorSettings& settings);

		// Writes only the session into the file, keeping everything else as saved (so unsaved edits
		// in the Editor Settings window aren't written by accident). Doesn't log on success.
		static bool SaveSession(const EditorSession& session, const std::filesystem::path& path);

		// Reads just the font size. Safe before the log exists (CreateApplication): never logs.
		static bool ReadFontSize(const std::filesystem::path& path, float& fontSize);
	};
}
