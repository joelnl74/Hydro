#pragma once
#include "Hydro.h"
#include "Game.h"

namespace Hydro
{
	class TestLayer : public Layer
	{
	public:
		TestLayer();
		virtual ~TestLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate() override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		Scene* m_Scene;
		Game* game;
	};
}

