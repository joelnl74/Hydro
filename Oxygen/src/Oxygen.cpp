#include "Hydro.h"

namespace Hydro
{
	class Oxygen : public Application
	{
	public:
		Oxygen()
		{
		}
		~Oxygen()
		{
		}
	};

	Application* CreateApplication()
	{
		return new Oxygen();
	}
}
