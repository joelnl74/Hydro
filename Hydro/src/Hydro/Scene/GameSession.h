// Hydro/Scene/GameSession.h
#pragma once

#include "Hydro/Core/Assert.h"

#include <type_traits>

namespace Hydro
{
	// The state of one run of the game (party, inventory, quests), made at SceneStack::Start and destroyed at Stop, so
	// every run starts fresh. The game derives and registers its own type (docs/Scripting.md). Not a service locator.
	class GameSession
	{
	public:
		virtual ~GameSession() = default;

		// The session type the next run makes. Without one, runs get a plain GameSession.
		template<typename T>
		static void Register()
		{
			static_assert(std::is_base_of_v<GameSession, T>, "A session type derives from GameSession");
			SetFactory([]() -> GameSession* { return new T(); });
		}

		// The running session as the game's type. Only while a game runs; asserts otherwise.
		template<typename T>
		static T& Get()
		{
			T* session = dynamic_cast<T*>(GetActive());
			HY_CORE_ASSERT(session != nullptr,
				"GameSession::Get: no game is running, or its session isn't of this type (GameSession::Register)");
			return *session;
		}

		// The running session; nullptr while no game runs (editing).
		static GameSession* GetActive();
		static bool IsRunning()
		{
			return GetActive() != nullptr;
		}

	protected:
		// When the run starts (before the first scene's scripts) and ends (after every scene of the run is gone).
		virtual void OnStart() {}
		virtual void OnStop() {}

	private:
		friend class SceneStack;

		using Factory = GameSession* (*)();
		static void SetFactory(Factory factory);

		// SceneStack::Start / Stop. Starting while a session runs ends that one first.
		static void Start();
		static void Stop();
	};
}
