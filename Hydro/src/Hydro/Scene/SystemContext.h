#pragma once
#include <cstdint>

namespace Hydro
{
	// What a scene system is told about the frame it runs in. Every system takes
	// (Scene&, const SystemContext&), so a new piece of frame data is one more field here rather than
	// one more parameter on every system. Scene builds it once per update (OnUpdateRuntime /
	// OnUpdateEditor) and passes the same one to each stage, so all stages of a frame agree.
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
		// The game is running (Scene::OnUpdateRuntime). False while editing. Game-only systems
		// return immediately when it is false.
		bool IsPlaying = false;
	};
}
