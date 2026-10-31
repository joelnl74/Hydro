#include "hypch.h"
#include "DataReport.h"

namespace Hydro
{
	std::string DataMessage::ToString() const
	{
		std::string where = Row;
		if (Path.empty() == false)
		{
			if (where.empty() == false)
			{
				where += ".";
			}
			where += Path;
		}

		if (where.empty())
		{
			return Text;
		}
		return where + ": " + Text;
	}

	void DataReport::AddError(const std::string& row, const std::string& path, const std::string& text)
	{
		m_Messages.push_back(DataMessage{ DataMessageSeverity::Error, row, path, text });
	}

	void DataReport::AddWarning(const std::string& row, const std::string& path, const std::string& text)
	{
		m_Messages.push_back(DataMessage{ DataMessageSeverity::Warning, row, path, text });
	}

	size_t DataReport::GetErrorCount() const
	{
		size_t count = 0;
		for (const DataMessage& message : m_Messages)
		{
			if (message.Severity == DataMessageSeverity::Error)
			{
				count++;
			}
		}
		return count;
	}

	size_t DataReport::GetWarningCount() const
	{
		return m_Messages.size() - GetErrorCount();
	}

	void DataReport::Append(const DataReport& other)
	{
		m_Messages.insert(m_Messages.end(), other.m_Messages.begin(), other.m_Messages.end());
	}

	void DataReport::Log(const std::string& source) const
	{
		for (const DataMessage& message : m_Messages)
		{
			if (message.Severity == DataMessageSeverity::Error)
			{
				HY_CORE_ERROR("{}: {}", source, message.ToString());
			}
			else
			{
				HY_CORE_WARN("{}: {}", source, message.ToString());
			}
		}
	}

	static std::string CountText(size_t count, const char* noun)
	{
		return std::to_string(count) + " " + noun + (count == 1 ? "" : "s");
	}

	std::string DataReport::GetSummary() const
	{
		const size_t errors = GetErrorCount();
		const size_t warnings = GetWarningCount();
		if (errors == 0 && warnings == 0)
		{
			return "Valid";
		}
		if (errors == 0)
		{
			return CountText(warnings, "warning");
		}
		if (warnings == 0)
		{
			return CountText(errors, "error");
		}
		return CountText(errors, "error") + ", " + CountText(warnings, "warning");
	}
}
