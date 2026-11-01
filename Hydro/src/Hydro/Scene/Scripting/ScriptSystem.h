#pragma once

namespace Hydro
{
	class Scene;
	struct SystemContext;

	// System (play mode, first stage): runs every NativeScriptComponent's scripts. Each update, in three
	// passes (Scripting plan SC3): every script without an instance is created and given its properties;
	// then the new scripts' OnCreate run; then every script's OnUpdate(context.DeltaTime), in bind order.
	// So an OnCreate can find a script on any entity of the scene (Entity::GetComponent<T>), with its
	// properties set, though that script's own OnCreate may not have run yet.
	// Does nothing unless context.IsPlaying.
	//
	// Safe against what scripts do to the scene (#40): the loop walks a snapshot of the entities, and
	// Scene::DestroyEntity / RemoveScript / RemoveScripts are queued until the stage ends (see
	// Scene::IsPendingDestroy). Entities and scripts created during the stage start next update.
	void UpdateScripts(Scene& scene, const SystemContext& context);
}
