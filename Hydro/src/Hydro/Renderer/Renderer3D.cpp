#include "hypch.h"
#include "Renderer3D.h"

namespace Hydro
{
	static bool s_Initialized = false;

	void Renderer3D::Init()
	{
		HY_CORE_ASSERT(s_Initialized == false, "Renderer3D::Init called twice");
		s_Initialized = true;
	}

	void Renderer3D::Shutdown()
	{
		s_Initialized = false;
	}
}
