#include "hypch.h"
#include "Log.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/pattern_formatter.h"

#include <deque>
#include <filesystem>
#include <mutex>

namespace Hydro
{
	// Keeps the last Log::MaxKeptMessages messages of both loggers in memory (a ring buffer), so the
	// application can show them. spdlog calls sink_it_ with the sink's mutex held.
	class LogHistorySink : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		LogHistorySink()
			: m_TimeFormatter("%T", spdlog::pattern_time_type::local, std::string())
		{
		}

		uint64_t GetMessagesSince(uint64_t afterSequence, std::vector<LogMessage>& out)
		{
			std::lock_guard<std::mutex> lock(mutex_);

			for (const LogMessage& message : m_Messages)
			{
				if (message.Sequence > afterSequence)
					out.push_back(message);
			}

			return m_Messages.empty() ? afterSequence : std::max(afterSequence, m_Messages.back().Sequence);
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			LogMessage message;
			message.Sequence = ++m_LastSequence;
			message.Level = ToLogLevel(msg.level);
			message.Source.assign(msg.logger_name.data(), msg.logger_name.size());
			message.Text.assign(msg.payload.data(), msg.payload.size());

			spdlog::memory_buf_t time;
			m_TimeFormatter.format(msg, time);
			message.Time.assign(time.data(), time.size());

			m_Messages.push_back(std::move(message));
			if (m_Messages.size() > Log::MaxKeptMessages)
				m_Messages.pop_front();
		}

		void flush_() override
		{
		}

	private:
		static LogLevel ToLogLevel(spdlog::level::level_enum level)
		{
			switch (level)
			{
			case spdlog::level::trace:
			case spdlog::level::debug:    return LogLevel::Trace;
			case spdlog::level::info:     return LogLevel::Info;
			case spdlog::level::warn:     return LogLevel::Warn;
			case spdlog::level::err:      return LogLevel::Error;
			case spdlog::level::critical: return LogLevel::Fatal;
			default:                      return LogLevel::Info;
			}
		}

	private:
		spdlog::pattern_formatter m_TimeFormatter;
		std::deque<LogMessage> m_Messages;
		uint64_t m_LastSequence = 0;
	};

	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
	std::shared_ptr<LogHistorySink> Log::s_History;

	void Log::Init()
	{
		// Create "logs" directory if doesn't exist
		std::string logsDirectory = "logs";
		if (std::filesystem::exists(logsDirectory) == false)
			std::filesystem::create_directories(logsDirectory);

		// One history shared by both loggers, so the order between engine and client lines is kept.
		s_History = std::make_shared<LogHistorySink>();

		std::vector<spdlog::sink_ptr> HydroSinks =
		{
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/Hydro.log", true)
		};

		std::vector<spdlog::sink_ptr> appSinks =
		{
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/APP.log", true)
		};

		HydroSinks[0]->set_pattern("%^[%T] %n: %v%$");
		HydroSinks[1]->set_pattern("[%T] [%l] %n: %v");
		appSinks[0]->set_pattern("%^[%T] %n: %v%$");
		appSinks[1]->set_pattern("[%T] [%l] %n: %v");

		HydroSinks.push_back(s_History);
		appSinks.push_back(s_History);

		s_CoreLogger = std::make_shared<spdlog::logger>("Hydro", HydroSinks.begin(), HydroSinks.end());
		s_CoreLogger->set_level(spdlog::level::trace);

		s_ClientLogger = std::make_shared<spdlog::logger>("APP", appSinks.begin(), appSinks.end());
		s_ClientLogger->set_level(spdlog::level::trace);
	}

	void Log::Shutdown()
	{
		s_ClientLogger.reset();
		s_CoreLogger.reset();
		s_History.reset();
		spdlog::drop_all();
	}

	uint64_t Log::GetMessagesSince(uint64_t afterSequence, std::vector<LogMessage>& out)
	{
		if (s_History == nullptr)
			return afterSequence;

		return s_History->GetMessagesSince(afterSequence, out);
	}
}
