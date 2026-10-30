#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include "Hydro/Core/Base.h"
#include "Hydro/Scene/SceneTransition.h"

namespace Hydro
{
	class Scene;

	// The scenes of a running game: the top one updates and draws, the ones below it wait exactly as they
	// were (entities, scripts, physics bodies). Owned by whatever runs the game (the editor's Play mode, the
	// standalone Runtime), which calls ProcessTransition after each frame's update to do what scripts asked
	// for with SceneTransition.
	class SceneStack
	{
	public:
		// Starts a game on `scene` (already Init'ed): it becomes the only scene and starts running
		// (Scene::OnRuntimeStart). A pending SceneTransition request is forgotten.
		void Start(const Ref<Scene>& scene);
		// Ends the game: the running scene stops (Scene::OnRuntimeStop) and every scene goes, the top one
		// first. Nothing happens when it isn't running.
		void Stop();

		// Takes the pending SceneTransition request and does it: Push loads
		// <scenesDirectory>/<name>.hyscene on top (sized to viewportWidth x viewportHeight; nothing changes when
		// it can't be loaded), Pop throws the top scene away. Quit is only reported: what quitting means is up
		// to the host (the Runtime closes, the editor stops playing). Returns what was done: None when there
		// was no request or it failed (logged).
		SceneTransition::Type ProcessTransition(const std::filesystem::path& scenesDirectory, uint32_t viewportWidth, uint32_t viewportHeight);

		// The scene that runs; null when the game isn't running.
		const Ref<Scene>& GetActiveScene() const { return m_Active; }
		bool IsRunning() const { return m_Active != nullptr; }
		// Scenes below the running one.
		uint32_t GetDepth() const { return (uint32_t)m_Suspended.size(); }
		// Every scene of the game, bottom first, the running one last (empty when not running).
		std::vector<Ref<Scene>> GetScenes() const;

	private:
		Ref<Scene> m_Active;
		std::vector<Ref<Scene>> m_Suspended; // bottom first
	};
}
