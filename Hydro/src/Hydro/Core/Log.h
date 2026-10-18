#pragma once
#include "Hydro/Core/Base.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// Core Logging Macros
	#define HY_CORE_TRACE(...)	Hydro::Log::GetCoreLogger()->trace(__VA_ARGS__)
	#define HY_CORE_INFO(...)	Hydro::Log::GetCoreLogger()->info(__VA_ARGS__)
	#define HY_CORE_WARN(...)	Hydro::Log::GetCoreLogger()->warn(__VA_ARGS__)
	#define HY_CORE_ERROR(...)	Hydro::Log::GetCoreLogger()->error(__VA_ARGS__)
	#define HY_CORE_FATAL(...)	Hydro::Log::GetCoreLogger()->critical(__VA_ARGS__)

	// Client Logging Macros
	#define HY_TRACE(...)	Hydro::Log::GetClientLogger()->trace(__VA_ARGS__)
	#define HY_INFO(...)	Hydro::Log::GetClientLogger()->info(__VA_ARGS__)
	#define HY_WARN(...)	Hydro::Log::GetClientLogger()->warn(__VA_ARGS__)
	#define HY_ERROR(...)	Hydro::Log::GetClientLogger()->error(__VA_ARGS__)
	#define HY_FATAL(...)	Hydro::Log::GetClientLogger()->critical(__VA_ARGS__)

	enum class LogLevel : uint8_t
	{
		Trace = 0,
		Info,
		Warn,
		Error,
		Fatal
	};

	// One logged line, kept in memory so it can be shown in the application (the editor's Console).
	struct LogMessage
	{
		uint64_t Sequence = 0;   // 1, 2, 3, ... in logging order, across both loggers
		LogLevel Level = LogLevel::Info;
		std::string Source;      // logger name: "Hydro" (engine) or "APP" (client)
		std::string Time;        // local time, HH:MM:SS
		std::string Text;
	};

	class LogHistorySink;

	class Log
	{
	public:
		static void Init();
		static void Shutdown();

		// Appends every message logged after `afterSequence` (0 = all that are kept) to `out`, oldest
		// first, and returns the sequence of the newest one (or `afterSequence` if there are none).
		// The last MaxKeptMessages messages are kept. Safe to call from any thread.
		static uint64_t GetMessagesSince(uint64_t afterSequence, std::vector<LogMessage>& out);
		static constexpr size_t MaxKeptMessages = 5000;

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
		static std::shared_ptr<LogHistorySink> s_History;
	};
}

