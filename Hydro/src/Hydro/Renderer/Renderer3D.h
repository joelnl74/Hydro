#pragma once

namespace Hydro
{
	class Renderer3D
	{
	public:
		static void Init();
		static void ShutDown();

		static void Begin();
		static void End();
		static void DrawMesh();
	};
}
