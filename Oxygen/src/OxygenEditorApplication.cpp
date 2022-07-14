#include "Hydro.h"

namespace Hydro
{
	class OxygenEditorApplication : public Application
	{
	public:
		OxygenEditorApplication()
		{
		}
		~OxygenEditorApplication()
		{
		}
	};

	Application* CreateApplication()
	{
		return new OxygenEditorApplication();
	}
}
