#include "hypch.h"
#include "Time.h"

#include <chrono>

namespace Hydro
{
	using Clock = std::chrono::steady_clock;

	struct TimeData
	{
		Clock::time_point StartTime;
		Clock::time_point FrameStart;
		std::array<Clock::time_point, (size_t)FramePhase::Count> PhaseStart{};

		bool HasPreviousFrame = false;
		float DeltaTime = 0.0f;

		FrameTimings Current;   // being filled this frame
		FrameTimings LastFrame; // what GetFrameTimings() returns
	};

	static TimeData* s_Data = nullptr;

	static float MillisecondsBetween(Clock::time_point from, Clock::time_point to)
	{
		return std::chrono::duration<float, std::milli>(to - from).count();
	}

	void Time::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "Time::Init called twice");

		s_Data = new TimeData();
		s_Data->StartTime = Clock::now();
	}

	void Time::Shutdown()
	{
		delete s_Data;
		s_Data = nullptr;
	}

	void Time::BeginFrame()
	{
		const Clock::time_point now = Clock::now();

		s_Data->Current = FrameTimings();
		s_Data->Current.FrameCount = s_Data->LastFrame.FrameCount + 1;

		if (s_Data->HasPreviousFrame)
		{
			const float deltaMs = MillisecondsBetween(s_Data->FrameStart, now);
			s_Data->Current.DeltaMs = deltaMs;
			s_Data->DeltaTime = std::min(deltaMs / 1000.0f, MaxDeltaTime);
		}

		s_Data->FrameStart = now;
		s_Data->HasPreviousFrame = true;
	}

	void Time::EndFrame()
	{
		const Clock::time_point now = Clock::now();

		s_Data->Current.FrameMs = MillisecondsBetween(s_Data->FrameStart, now);
		s_Data->Current.UptimeSeconds = std::chrono::duration<double>(now - s_Data->StartTime).count();
		s_Data->LastFrame = s_Data->Current;
	}

	void Time::BeginPhase(FramePhase phase)
	{
		s_Data->PhaseStart[(size_t)phase] = Clock::now();
	}

	void Time::EndPhase(FramePhase phase)
	{
		s_Data->Current.PhaseMs[(size_t)phase] += MillisecondsBetween(s_Data->PhaseStart[(size_t)phase], Clock::now());
	}

	void Time::SetPhaseMs(FramePhase phase, float ms)
	{
		s_Data->Current.PhaseMs[(size_t)phase] = ms;
	}

	float Time::GetDeltaTime()
	{
		return s_Data->DeltaTime;
	}

	uint64_t Time::GetFrameCount()
	{
		return s_Data->LastFrame.FrameCount;
	}

	double Time::GetUptime()
	{
		return std::chrono::duration<double>(Clock::now() - s_Data->StartTime).count();
	}

	const FrameTimings& Time::GetFrameTimings()
	{
		return s_Data->LastFrame;
	}
}
