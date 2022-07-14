#include "Hydro.h"

namespace Hydro
{
	class Oxygen : public Application
	{
	public:
		Oxygen()
			: Application()
		{
		}
	};

	void main()
	{
		Oxygen* oxygenApp = new Oxygen();
		delete oxygenApp;
	}
}
