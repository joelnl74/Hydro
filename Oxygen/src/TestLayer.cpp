#include "TestLayer.h"
#include <iostream>

namespace Hydro
{
	TestLayer::TestLayer()
	{
	}

	TestLayer::~TestLayer()
	{
	}

	void TestLayer::OnAttach()
	{
	}

	void TestLayer::OnDetach()
	{
	}

	void TestLayer::OnUpdate()
	{
		if (Input::IsKeyPressed(KeyCode::A))
		{
			std::cout << "A" << std::endl;
		}
	}
	void TestLayer::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::MouseMoved)
		{
			auto derrivedEvent = EventDispatcher::Get().GetEvent<MouseMovedEvent>(e);
			std::cout << derrivedEvent.GetX() << ", " << derrivedEvent.GetY() << std::endl;
		}
	}
}


