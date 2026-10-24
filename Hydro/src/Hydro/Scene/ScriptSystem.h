#pragma once

namespace Hydro
{
	class Scene;
	struct SystemContext;

	// System (play mode, first stage): runs every NativeScriptComponent's scripts. Instances are
	// created on their first update (OnCreate), then OnUpdate(context.DeltaTime) runs in bind order.
	// Does nothing unless context.IsPlaying.
	//
	// Safe against what scripts do to the scene (#40): the loop walks a snapshot of the entities, and
	// Scene::DestroyEntity / RemoveScript / RemoveScripts are queued until the stage ends (see
	// Scene::IsPendingDestroy). Entities and scripts created during the stage start next update.
	void UpdateScripts(Scene& scene, const SystemContext& context);
}
