#include <iostream>

#include <Hydro\Core\EntryPoint.h>
#include "Hydro.h"

#include "TestLayer.h"

class OxygenEditorApplication : public Hydro::Application
{
public:
	OxygenEditorApplication() : Application()
	{
	}
	~OxygenEditorApplication()
	{
	}

	virtual void OnInit() override 
	{
		PushLayer(new Hydro::TestLayer());
	}
};

Hydro::Application* Hydro::CreateApplication()
{
	return new OxygenEditorApplication();
}
