#include "hypch.h"
#include "ScriptProperty.h"

namespace Hydro
{
	const char* ScriptPropertyTypeToString(ScriptPropertyType type)
	{
		switch (type)
		{
			case ScriptPropertyType::Bool: return "bool";
			case ScriptPropertyType::Int: return "int";
			case ScriptPropertyType::Float: return "float";
			case ScriptPropertyType::Vec2: return "glm::vec2";
			case ScriptPropertyType::Vec3: return "glm::vec3";
			case ScriptPropertyType::Vec4: return "glm::vec4";
			case ScriptPropertyType::String: return "std::string";
			case ScriptPropertyType::Asset: return "AssetHandle";
		}
		return "unknown";
	}
}
