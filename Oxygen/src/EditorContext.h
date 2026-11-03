#pragma once
#include "EditorCommands/EditorCommand.h"
#include "EditorSettings.h"
#include "EditorWindows/TerrainTool.h"

#include <Hydro/Core/Base.h>
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/Scene.h>

#include <cstdint>
#include <filesystem>
#include <functional>

namespace Hydro
{
	// The view captures asked for (F9, the Capture menu, --capture). A request waits until its view is drawn.
	struct CaptureRequests
	{
		static constexpr uint32_t FixedWidth = 1280;
		static constexpr uint32_t FixedHeight = 720;

		bool Game = false;
		bool Editor = false;
		// Where the Game view's capture goes (--capture); empty: a new file in the project's captures folder.
		std::filesystem::path GamePath;
		// Render the captured frame at FixedWidth x FixedHeight, whatever the window's size.
		bool FixedSize = true;
		bool KeyDown = false; // F9 last frame, so holding it captures once
	};

	// What the editor's windows share: the scenes, the selection, the play state, the settings and the undo history.
	// EditorLayer owns it and does what changes the scenes (open, save, play, stop); windows read it and edit through it.
	struct EditorContext
	{
		Ref<Scene> EditorScene;  // what is edited and saved
		Ref<Scene> ActiveScene;  // the running scene while playing, else EditorScene
		std::filesystem::path EditorScenePath; // empty until the scene is saved or opened

		bool Playing = false;
		bool Paused = false;
		uint32_t StepFrames = 0; // runtime frames still to run while paused (Step button)

		// The selected entity, in ActiveScene (the hierarchy drops it when it isn't in that scene any more).
		Entity Selection;
		// The asset the inspector shows while no entity is selected (picked in the Project window).
		std::filesystem::path InspectedAsset;

		// Loaded from EditorSettings.yaml when the editor starts; the session part is kept up to date and written on close.
		EditorSettings Settings;
		// The undo history (#165) and the Terrain tool, whose strokes are its first commands.
		EditorCommandStack Commands;
		TerrainTool Terrain;
		CaptureRequests Captures;

		// A prefab, scene or model dropped from the Project window onto the Editor or Game view.
		std::function<void(const std::filesystem::path&)> OnAssetDropped;

		// Makes the last ImGui item take those drops.
		void AcceptAssetDrop();
	};
}
