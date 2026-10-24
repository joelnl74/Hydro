#pragma once

namespace Hydro
{
	class Scene;
	struct SystemContext;

	// System (play mode, first stage): runs every NativeScriptComponent's scripts. Instances are
	// created on their first update (OnCreate), then OnUpdate(context.DeltaTime) runs in bind order.
	// Does nothing unless context.IsPlaying.
	// TODO(#40): the loop iterates the live registry view, so a script must not add or remove
	// NativeScriptComponents or destroy entities; the safe stage replaces this.
	void UpdateScripts(Scene& scene, const SystemContext& context);
}
