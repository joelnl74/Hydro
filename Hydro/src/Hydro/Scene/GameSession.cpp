#include "hypch.h"
#include "GameSession.h"

#include <memory>

namespace Hydro
{
	struct GameSessionData
	{
		GameSession* (*Factory)() = nullptr;
		std::unique_ptr<GameSession> Active;
	};

	// Created on first use, so registering from anywhere (even another static) is safe.
	static GameSessionData& GetData()
	{
		static GameSessionData data;
		return data;
	}

	void GameSession::SetFactory(Factory factory)
	{
		HY_CORE_ASSERT(factory != nullptr, "GameSession::Register: no factory");
		GetData().Factory = factory;
	}

	GameSession* GameSession::GetActive()
	{
		return GetData().Active.get();
	}

	void GameSession::Start()
	{
		Stop();

		GameSessionData& data = GetData();
		data.Active.reset(data.Factory != nullptr ? data.Factory() : new GameSession());
		data.Active->OnStart();
	}

	void GameSession::Stop()
	{
		GameSessionData& data = GetData();
		if (data.Active == nullptr)
		{
			return;
		}

		data.Active->OnStop();
		data.Active.reset();
	}
}
