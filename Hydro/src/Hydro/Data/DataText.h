// Hydro/Data/DataText.h
#pragma once

#include "Hydro/Data/DataType.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Hydro
{
	// Paths and text for the data files and their messages; private to DataReader, DataWriter and DataChecks.
	class DataText
	{
	public:
		static std::string JoinPath(const std::string& parent, const std::string& child)
		{
			if (parent.empty())
			{
				return child;
			}
			return parent + "." + child;
		}

		static std::string IndexKey(const std::string& key, size_t index)
		{
			return key + "[" + std::to_string(index) + "]";
		}

		// What a node looks like, for messages: the scalar itself, or "a list" / "a map".
		static std::string Describe(const YAML::Node& node)
		{
			if (node.IsScalar())
			{
				return "'" + node.Scalar() + "'";
			}
			if (node.IsSequence())
			{
				return "a list";
			}
			if (node.IsMap())
			{
				return "a map";
			}
			return "nothing";
		}

		// The shortest text that reads back as exactly `value` (0.1f -> "0.1", not yaml-cpp's "0.100000001").
		static std::string FormatFloat(float value)
		{
			if (std::isnan(value))
			{
				return ".nan";
			}
			if (std::isinf(value))
			{
				return value > 0.0f ? ".inf" : "-.inf";
			}

			char buffer[32] = {};
			for (int precision = 6; precision <= 9; precision++)
			{
				std::snprintf(buffer, sizeof(buffer), "%.*g", precision, (double)value);
				if (std::strtof(buffer, nullptr) == value)
				{
					break;
				}
			}
			return buffer;
		}

		static std::string FormatNumber(float value)
		{
			// Range limits come in as floats; show whole numbers without a fraction.
			if (value == std::floor(value) && std::fabs(value) < 1.0e9f)
			{
				return std::to_string((long long)value);
			}
			return FormatFloat(value);
		}

		static const char* ExpectedText(DataFieldKind kind)
		{
			switch (kind)
			{
				case DataFieldKind::Bool: return "true or false";
				case DataFieldKind::Int: return "a whole number";
				case DataFieldKind::Float: return "a number";
				case DataFieldKind::Vec2: return "[x, y]";
				case DataFieldKind::Vec3: return "[x, y, z]";
				case DataFieldKind::Vec4: return "[x, y, z, w]";
				case DataFieldKind::String: return "text";
				case DataFieldKind::Asset: return "an asset handle (a number, 0 = none)";
				case DataFieldKind::Enum: return "an enumerator's name";
				case DataFieldKind::Struct: return "a map of fields";
				case DataFieldKind::List: return "a list";
				case DataFieldKind::RowRef: return "{ Table: <handle>, Row: <id> }";
				case DataFieldKind::EntityRef: return "an entity's UUID (a number, 0 = none)";
			}
			return "a value";
		}
	};
}
