#pragma once
#include <cstdint>
#include <array>

namespace Hydro
{
	// The CPU phases of one frame, in the order Application::Run executes them.
	enum class FramePhase : uint8_t
	{
		Events = 0,      // window/OS event polling
		ImGui,           // building the UI (layers' OnImGuiRender)
		Update,          // game/scene logic (layers' OnUpdate)
		Render,          // frame fence wait + acquire + recording (reported by the Renderer)
		Present,         // queue submit + present, includes the vsync wait (reported by the Renderer)
		PlatformWindows, // ImGui windows dragged outside the main window
		Count
	};

	// CPU time spent in each phase of a frame, in milliseconds.
	struct FrameTimings
	{
		std::array<float, (size_t)FramePhase::Count> PhaseMs{};

		float FrameMs = 0.0f;       // whole loop iteration (BeginFrame -> EndFrame)
		float DeltaMs = 0.0f;       // start of the previous frame -> start of this one, unclamped
		uint64_t FrameCount = 0;    // frames completed since Time::Init
		double UptimeSeconds = 0.0; // since Time::Init

		float GetPhaseMs(FramePhase phase) const { return PhaseMs[(size_t)phase]; }
	};

	// Time subsystem: the frame clock (delta time, frame count, uptime) and per-phase CPU timings.
	// Like Log, it is a static facade with an explicit lifetime: Application calls Init() first thing
	// and Shutdown() last, and drives BeginFrame/EndFrame once per loop iteration.
	//
	//   float dt = Time::GetDeltaTime();                        // seconds, for gameplay
	//   const FrameTimings& t = Time::GetFrameTimings();        // last complete frame, for stats UI
	//   float renderMs = t.GetPhaseMs(FramePhase::Render);
	class Time
	{
	public:
		// Delta time is clamped to this, so a breakpoint or a long hitch doesn't make the
		// simulation jump. FrameTimings::DeltaMs keeps the real value.
		static constexpr float MaxDeltaTime = 0.1f;

		static void Init();
		static void Shutdown();

		// Application::Run only.
		static void BeginFrame();
		static void EndFrame();

		// Everything between BeginPhase and EndPhase is charged to that phase of the current frame.
		static void BeginPhase(FramePhase phase);
		static void EndPhase(FramePhase phase);
		// For phases measured elsewhere (the Renderer times its own recording and present).
		static void SetPhaseMs(FramePhase phase, float ms);

		// Seconds between the start of the previous frame and this one, clamped to MaxDeltaTime.
		// 0 on the first frame.
		static float GetDeltaTime();
		static uint64_t GetFrameCount();
		// Seconds since Init.
		static double GetUptime();

		// The last complete frame. Readers (e.g. the editor's stats window) never see a half-filled frame.
		static const FrameTimings& GetFrameTimings();
	};
}
