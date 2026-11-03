#pragma once
#include <cstdint>

namespace Hydro
{
	class Scene;
	// What a scene system is told about its frame. Every system takes (Scene&, const SystemContext&); Scene builds one
	// per update and passes it to each stage, so all stages of a frame agree.
	struct SystemContext
	{
		// The step the fixed-timestep stages (physics) advance by, in seconds: 60 Hz.
		static constexpr float DefaultFixedDeltaTime = 1.0f / 60.0f;

		// Seconds since the previous frame (Time::GetDeltaTime(), already clamped).
		float DeltaTime = 0.0f;
		// The fixed step, for stages that accumulate DeltaTime and step in equal slices.
		float FixedDeltaTime = DefaultFixedDeltaTime;
		// Frames completed since Time::Init (Time::GetFrameCount()).
		uint64_t FrameIndex = 0;
		// False while editing; game-only systems return at once then.
		bool IsPlaying = false;
	};

	// One step of a scene update. Scene::GetStages lists them in order; OnUpdateRuntime runs every one, OnUpdateEditor
	// those that RunsInEditor.
	struct SceneStage
	{
		const char* Name = "";
		bool RunsInEditor = false;
		void (*Run)(Scene& scene, const SystemContext& context) = nullptr;
	};
}
