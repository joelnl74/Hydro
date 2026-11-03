#pragma once

namespace Hydro
{
	class Scene;
	struct SystemContext;

	// Play mode, first stage: creates new scripts with their properties, runs their OnCreate, then every OnUpdate in
	// bind order. Scene changes from scripts are deferred to the stage's end (#40); new entities start next update.
	void UpdateScripts(Scene& scene, const SystemContext& context);
}
