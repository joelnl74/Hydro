#pragma once

#include "Hydro.h"

#include <filesystem>
#include <string>

// The run's state in a save file, and back: game code on yaml-cpp. The engine only switches scenes for it
// (SceneTransition::Replace and Restart).

namespace Hydro
{
	// Where the player stood when the game was saved; a loaded save waits here until that scene's field director
	// places her (RpgSession::Arrival).
	struct SavePlace
	{
		std::string Scene;          // a scene file name ("FarmVillage"); empty: none waiting
		glm::vec3 Position{ 0.0f }; // world position of the "Player" entity
	};

	// One save slot: <project folder>/Saves/Slot<n>.hysave, YAML. It holds the party, the inventory, the quest log, the
	// weaknesses found, the time of day and where the player stood.
	class SaveGame
	{
	public:
		static constexpr int Version = 1;

		// What a slot holds, for the menu, without loading it.
		struct Summary
		{
			bool Exists = false;
			std::string Scene;
			std::string Party;  // "Miko Lv 3, Orin Lv 2"
			std::string Saved;  // local time, year-month-day hours:minutes
			int Money = 0;
		};

		static std::filesystem::path GetPath(int slot = 1);
		static bool Exists(int slot = 1);
		static Summary ReadSummary(int slot = 1);

		// The running session's state; false (with a warning) when the file can't be written.
		static bool Write(const SavePlace& place, int slot = 1);
		// Into the running session, replacing its state; `place` is where the save was made. False (with a warning)
		// when there is no readable save, leaving the session as it was.
		static bool Load(int slot, SavePlace& place);
		// Where a just-loaded save puts the player when `scene` is the one it was made in (once); false otherwise.
		static bool TakeArrival(const std::string& scene, glm::vec3& position);
		// False when there was nothing to delete.
		static bool Delete(int slot = 1);
	};
}
