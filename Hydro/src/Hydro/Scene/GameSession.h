// Hydro/Scene/GameSession.h
#pragma once

#include "Hydro/Core/Assert.h"

#include <type_traits>

namespace Hydro
{
	// The state of one run of the game (Scripting plan SC2). A session is made when a game starts and destroyed when
	// it ends (SceneStack::Start / Stop: the editor's Play and Stop, the standalone player's start and quit), and
	// survives every scene change on the scene stack in between. So a new run always starts fresh, and nothing a
	// run leaves behind survives Stop and Play.
	//
	// The game derives its own session type, registers it once at startup (like its scripts) and keeps its run state
	// there: the party, the inventory, the quest log, the battle hand-off.
	//
	//     class RpgSession : public GameSession { public: QuestLog Quests; ... };
	//     GameSession::Register<RpgSession>();               // OnInit
	//     GameSession::Get<RpgSession>().Quests.Start(...);  // while the game runs
	//
	// It holds run state and run-scoped game infrastructure only. It is not a service locator: rendering, input,
	// assets, audio and scenes keep their own engine APIs. One session type per game.
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

		// The running session as the game's type. Only while a game runs (scripts' OnCreate, OnUpdate, OnDestroy
		// of a running scene, UI and physics events); asserts otherwise.
		template<typename T>
		static T& Get()
		{
			T* session = dynamic_cast<T*>(GetActive());
			HY_CORE_ASSERT(session != nullptr, "GameSession::Get: no game is running, or its session isn't of this type (GameSession::Register)");
			return *session;
		}

		// The running session; nullptr while no game runs (editing).
		static GameSession* GetActive();
		static bool IsRunning()
		{
			return GetActive() != nullptr;
		}

	protected:
		// Called when the run starts, before the first scene's scripts are created, and when it ends, after every
		// scene of the run is gone.
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
