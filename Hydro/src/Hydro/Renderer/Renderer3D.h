#pragma once

namespace Hydro
{
	// Placeholder for the 3D path (Roadmap R9: meshes, MeshComponent, depth, a lit shader). A part of
	// Renderer, started and stopped by Renderer::Init / Shutdown. It owns nothing yet.
	class Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();
	};
}
