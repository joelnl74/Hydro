// Hydro/Data/DataRowRef.h
#pragma once

#include "Hydro/Asset/Asset.h"

#include <string>

namespace Hydro
{
	// Names one data table row (table asset + row ID); store this, never a pointer. The row ID is the row's lasting
	// identity. Saved as { Table: <handle>, Row: <id> }.
	struct DataRowRef
	{
		AssetHandle Table = 0;
		std::string Row;

		// Whether it names a row at all (not whether that row exists: Data::GetRow and validation say that).
		bool IsSet() const
		{
			return (uint64_t)Table != 0 && Row.empty() == false;
		}

		bool operator==(const DataRowRef& other) const
		{
			return (uint64_t)Table == (uint64_t)other.Table && Row == other.Row;
		}

		bool operator!=(const DataRowRef& other) const
		{
			return (*this == other) == false;
		}
	};
}
