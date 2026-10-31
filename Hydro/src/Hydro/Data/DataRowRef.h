// Hydro/Data/DataRowRef.h
#pragma once

#include "Hydro/Asset/Asset.h"

#include <string>

namespace Hydro
{
	// Names one row of a data table: the table asset and the row's ID. Scenes, other rows and (later) saves
	// store this, never a pointer: the row ID is the row's lasting identity, the table namespaces it
	// ("Items:Potion"). Saved as { Table: <handle>, Row: <id> }.
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
