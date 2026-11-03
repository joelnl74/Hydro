#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include "Hydro/Core/Base.h"
#include "Hydro/Scene/SceneTransition.h"

namespace Hydro
{
	class Scene;

	// The scenes of a running game: the top one updates and draws, the ones below wait as they were. Owned by the host,
	// which calls ProcessTransition after each frame's update.
	class SceneStack
	{
	public:
		// Starts a game on `scene` (already Init'ed) as the only scene; a pending SceneTransition request is forgotten.
		void Start(const Ref<Scene>& scene);
		// Stops the running scene and drops every scene, top first. Nothing happens when not running.
		void Stop();

		// Does the pending SceneTransition: Push loads <scenesDirectory>/<name>.hyscene on top, Pop drops the top scene,
		// Quit is only reported. Returns what was done (None when there was no request or it failed).
		SceneTransition::Type ProcessTransition(const std::filesystem::path& scenesDirectory, uint32_t viewportWidth,
			uint32_t viewportHeight);

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
