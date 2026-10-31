// Hydro/Data/DataReport.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	enum class DataMessageSeverity : uint8_t
	{
		Warning, // the data works, but something needs a look (an unknown kept key, a value outside its Range)
		Error    // something is broken (a missing row, an unknown enum name, a duplicate row ID)
	};

	// One problem found while reading or validating data, with where it is.
	struct DataMessage
	{
		DataMessageSeverity Severity = DataMessageSeverity::Error;
		// The table row it is in (its ID); empty for a definition or the file itself.
		std::string Row;
		// The field path inside the record: "Stock[2].Item"; empty for the file or the whole row.
		std::string Path;
		std::string Text;

		// "Potion.RestoreHP: 1200 is outside 0..999", "Lines[1].Speaker: ..." or just the text.
		std::string ToString() const;
	};

	// What reading or validating one file found. The editor's validation line counts these; the loader logs them
	// once (data breaks quietly otherwise).
	class DataReport
	{
	public:
		void AddError(const std::string& row, const std::string& path, const std::string& text);
		void AddWarning(const std::string& row, const std::string& path, const std::string& text);

		const std::vector<DataMessage>& GetMessages() const { return m_Messages; }
		size_t GetErrorCount() const;
		size_t GetWarningCount() const;
		bool HasErrors() const { return GetErrorCount() > 0; }
		bool IsEmpty() const { return m_Messages.empty(); }

		// "Valid", "2 warnings", "1 error, 2 warnings".
		std::string GetSummary() const;

	private:
		std::vector<DataMessage> m_Messages;
	};
}
