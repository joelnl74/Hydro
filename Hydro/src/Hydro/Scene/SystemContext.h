#pragma once
#include <cstdint>

namespace Hydro
{
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
}
