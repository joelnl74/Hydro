#include "hypch.h"
#include "Hydro/Utils/PlatformUtils.h"

#include <mutex>
#include <thread>
#include <vector>

namespace Hydro
{
	struct ProcessData
	{
		HANDLE ProcessHandle = nullptr;
		// Holds the program and everything it starts, so Kill stops the whole tree; closing it kills what is left.
		HANDLE Job = nullptr;
		HANDLE OutputRead = nullptr;
		std::thread Reader;
		std::mutex OutputMutex;
		std::string Output;
	};

	static std::wstring ToWide(const std::string& text)
	{
		if (text.empty())
		{
			return std::wstring();
		}

		const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
		std::wstring wide((size_t)length, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wide.data(), length);
		return wide;
	}

	static void CloseIfOpen(HANDLE& handle)
	{
		if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(handle);
		}
		handle = nullptr;
	}

	Process::Process(const ProcessSpecification& specification)
		: m_Data(CreateScope<ProcessData>())
	{
		// The program writes into a pipe that a reader thread empties; only the pipe's write end is inherited.
		SECURITY_ATTRIBUTES security = {};
		security.nLength = sizeof(security);
		security.bInheritHandle = TRUE;

		HANDLE outputWrite = nullptr;
		if (CreatePipe(&m_Data->OutputRead, &outputWrite, &security, 0) == FALSE)
		{
			HY_CORE_ERROR("Process: could not create a pipe for '{}' (error {})", specification.CommandLine, GetLastError());
			m_Data->OutputRead = nullptr;
			return;
		}
		SetHandleInformation(m_Data->OutputRead, HANDLE_FLAG_INHERIT, 0);

		// No input: a script that waits for a key gets end-of-file instead of hanging.
		HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);

		STARTUPINFOW startup = {};
		startup.cb = sizeof(startup);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = input;
		startup.hStdOutput = outputWrite;
		startup.hStdError = outputWrite;

		std::wstring commandLine = ToWide(specification.CommandLine);
		const std::wstring directory = specification.WorkingDirectory.wstring();
		PROCESS_INFORMATION info = {};
		const BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
			nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &info);

		// The program has its own copies of these now (or never started).
		CloseIfOpen(outputWrite);
		CloseIfOpen(input);

		if (started == FALSE)
		{
			HY_CORE_ERROR("Process: could not start '{}' (error {})", specification.CommandLine, GetLastError());
			CloseIfOpen(m_Data->OutputRead);
			return;
		}

		// In the job before it runs, so everything it starts is in it too.
		m_Data->Job = CreateJobObjectW(nullptr, nullptr);
		if (m_Data->Job != nullptr)
		{
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
			limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			SetInformationJobObject(m_Data->Job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
			if (AssignProcessToJobObject(m_Data->Job, info.hProcess) == FALSE)
			{
				CloseIfOpen(m_Data->Job);
			}
		}

		ResumeThread(info.hThread);
		CloseHandle(info.hThread);
		m_Data->ProcessHandle = info.hProcess;

		// Reads until every writer is gone (the program and anything it started that kept the pipe).
		ProcessData* data = m_Data.get();
		m_Data->Reader = std::thread([data]()
		{
			char buffer[4096];
			DWORD read = 0;
			while (ReadFile(data->OutputRead, buffer, sizeof(buffer), &read, nullptr) != FALSE && read > 0)
			{
				std::lock_guard<std::mutex> lock(data->OutputMutex);
				data->Output.append(buffer, read);
			}
		});
	}

	Process::~Process()
	{
		Kill();

		// Whatever the program left running in the job goes with it, which also ends the pipe.
		CloseIfOpen(m_Data->Job);
		if (m_Data->Reader.joinable())
		{
			// Something outside the job may still hold the pipe: don't wait for it.
			CancelSynchronousIo(m_Data->Reader.native_handle());
			m_Data->Reader.join();
		}

		CloseIfOpen(m_Data->OutputRead);
		CloseIfOpen(m_Data->ProcessHandle);
	}

	bool Process::IsValid() const
	{
		return m_Data->ProcessHandle != nullptr;
	}

	bool Process::IsRunning() const
	{
		return m_Data->ProcessHandle != nullptr && WaitForSingleObject(m_Data->ProcessHandle, 0) == WAIT_TIMEOUT;
	}

	int Process::GetExitCode() const
	{
		if (IsValid() == false || IsRunning())
		{
			return -1;
		}

		DWORD exitCode = 0;
		if (GetExitCodeProcess(m_Data->ProcessHandle, &exitCode) == FALSE)
		{
			return -1;
		}
		return (int)exitCode;
	}

	void Process::Kill()
	{
		if (IsRunning() == false)
		{
			return;
		}

		if (m_Data->Job != nullptr)
		{
			TerminateJobObject(m_Data->Job, 1);
		}
		else
		{
			TerminateProcess(m_Data->ProcessHandle, 1);
		}
		WaitForSingleObject(m_Data->ProcessHandle, 5000);
	}

	std::string Process::ReadOutput()
	{
		std::lock_guard<std::mutex> lock(m_Data->OutputMutex);
		std::string output;
		output.swap(m_Data->Output);
		return output;
	}

	bool Process::Launch(const ProcessSpecification& specification)
	{
		STARTUPINFOW startup = {};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION info = {};

		std::wstring commandLine = ToWide(specification.CommandLine);
		const std::wstring directory = specification.WorkingDirectory.wstring();
		if (CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr,
			directory.empty() ? nullptr : directory.c_str(), &startup, &info) == FALSE)
		{
			HY_CORE_ERROR("Could not start '{}' (error {})", specification.CommandLine, GetLastError());
			return false;
		}

		CloseHandle(info.hThread);
		CloseHandle(info.hProcess);
		return true;
	}

	std::filesystem::path Process::GetExecutablePath()
	{
		std::vector<wchar_t> buffer(MAX_PATH);
		while (true)
		{
			const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());
			if (length == 0)
			{
				return std::filesystem::path();
			}
			if (length < buffer.size())
			{
				return std::filesystem::path(std::wstring(buffer.data(), length));
			}
			buffer.resize(buffer.size() * 2);
		}
	}
}
