// Hydro/Data/DataReader.h
#pragma once

#include "Hydro/Data/DataRecord.h"
#include "Hydro/Data/DataReport.h"
#include "Hydro/Data/DataType.h"

#include <string>

namespace YAML
{
	class Node;
}

namespace Hydro
{
	struct DataReadContext
	{
		DataReport* Report = nullptr;
		std::string Row;
		// Duplicate keys or a row that isn't a map: writing would lose or merge something, so the file is read-only.
		bool StructureBroken = false;
		// Value problems (unreadable values, unknown keys) are reported by CheckRecord, not while loading, so the
		// same check serves a record in memory; it reads the kept value again with this on to say why.
		bool ReportValues = false;
		// A record keeps what it can't read (DataKeptValues); a value read on its own (ReadValue) doesn't.
		bool KeepsValues = true;
	};

	// Values and records read from YAML nodes; private to the data serializer (DataSerializer reads the files with
	// it, CheckRecord reads a kept value again to say why it is unreadable).
	class DataReader
	{
	public:
		// False when unreadable (reported here); the caller keeps the file's value and the field its default. `kept` is
		// the enclosing record's kept values, `keptKey` this value's key in it.
		static bool ReadValue(const YAML::Node& node, const DataValueType& valueType, void* value, DataKeptValues& kept,
			const std::string& keptKey, const std::string& path, DataReadContext& context);
		static void ReadStruct(const YAML::Node& node, const DataTypeInfo& type, void* object, DataKeptValues& kept,
			const std::string& path, DataReadContext& context);
	};
}
