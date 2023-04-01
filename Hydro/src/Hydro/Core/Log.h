#pragma once
#include "Hydro/Core/Base.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include <glm/glm.hpp>

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

	class Log
	{
	public:
		static void Init();
		static void Shutdown();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

