#pragma once
#include "EditorWindows/EditorStyle.h"

#include <Hydro/Renderer/EditorCamera.h>

#include <filesystem>
#include <map>
#include <string>

namespace Hydro
{
	// While the game runs, the editor's windows take on this colour: changes made now are thrown away on Stop.
	struct PlayModeSettings
	{
		bool Tint = true;
		ImVec4 TintColor = { 1.00f, 0.55f, 0.24f, 1.0f }; // ember
		float TintStrength = 0.12f;                       // 0 = none, 1 = the tint colour itself
		// Full screen (F11) also makes the editor's window cover the whole monitor, and leaves it when Play stops.
		bool FullscreenCoversMonitor = true;
		bool LeaveFullscreenOnStop = true;
	};

	// The transform gizmo's snap steps, used while Ctrl is held.
	struct GizmoSettings
	{
		float TranslateSnap = 0.5f; // world units
		float RotateSnap = 15.0f;   // degrees
		float ScaleSnap = 0.1f;
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

	// Saves the open scene every IntervalMinutes while editing (never while playing, only once it has a file).
	struct AutosaveSettings
	{
		bool Enabled = false;
		float IntervalMinutes = 5.0f;
	};

	// What the editor had open last time. Written by the editor, not edited by hand.
	struct EditorSession
	{
		std::filesystem::path LastProject;
		// The open project's last scene (absolute). Saved per project (EditorProjectState), not in the user's file.
		std::filesystem::path LastScene;
		// Open or closed per View menu window, by menu name. A window that isn't listed keeps its own default.
		std::map<std::string, bool> OpenWindows;
	};

	// The editor's own settings, per user in EditorSettings.yaml next to imgui.ini (not in the project).
	// Edited live in the Editor Settings window; Save there writes them.
	struct EditorSettings
	{
		EditorStyle Style = EditorStyle::FromTheme(EditorTheme::Hydro);
		// Pixel size of the UI font. The atlas is built at startup, so a change applies after a restart.
		float FontSize = 17.0f;

		PlayModeSettings PlayMode;

		EditorCameraSettings Camera;
		bool CameraStartsIn2D = true;
		GizmoSettings Gizmo;
		// The scene's look in the Editor view (Editor > Effects): off keeps it sharp; the Game view always shows it.
		bool EditorViewPostProcessing = false; // depth of field, bloom and the vignette
		bool EditorViewFog = false;

		ConsoleSettings Console;
		EditorStartupSettings Startup;
		AutosaveSettings Autosave;

		EditorSession Session;
	};

	// What the editor remembers about one project, in <project>/.hydro/Editor.yaml (EditorPaths).
	struct EditorProjectState
	{
		std::filesystem::path LastScene; // relative to the project folder
	};

	// Reads and writes EditorSettings as YAML. Missing keys keep their defaults.
	class EditorSettingsSerializer
	{
	public:
		// Version 2 added the themes and the accent colour; version 1 files keep their theme and font scale.
		static constexpr int Version = 2;

		static bool Save(const EditorSettings& settings, const std::filesystem::path& path);
		// Returns false (settings unchanged) when the file is missing or unreadable.
		static bool Load(const std::filesystem::path& path, EditorSettings& settings);

		// Writes only the session, keeping the rest as saved, so unsaved Editor Settings edits aren't written.
		static bool SaveSession(const EditorSession& session, const std::filesystem::path& path);

		// A project's editor state; false (state unchanged) when the file is missing or unreadable.
		static bool SaveProjectState(const EditorProjectState& state, const std::filesystem::path& path);
		static bool LoadProjectState(const std::filesystem::path& path, EditorProjectState& state);

		// Reads just the font size. Safe before the log exists (CreateApplication): never logs.
		static bool ReadFontSize(const std::filesystem::path& path, float& fontSize);
	};
}
