#pragma once
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace Hydro
{
	class ScriptableEntity;
	using ScriptFactory = ScriptableEntity* (*)();

	// Gives native scripts a name, so a scene can save which scripts an entity has and the editor
	// can offer them in "Add Script". Register every script before scenes are loaded (in
	// Application::OnInit, before pushing layers):
	//
	//     Hydro::ScriptRegistry::Register<PlayerController>("PlayerController");
	//
	// A lookup table, not a subsystem: it holds no OS or GPU resources and needs no Init/Shutdown.
	// Names must stay stable once scenes are saved with them (renaming a script breaks those files).
	class ScriptRegistry
	{
	public:
		template<typename T>
		static void Register(const std::string& name)
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "Scripts must derive from ScriptableEntity");
			Register(name, typeid(T), []() -> ScriptableEntity* { return new T(); });
		}

		static void Register(const std::string& name, std::type_index type, ScriptFactory factory);

		// The factory registered under `name`, or nullptr.
		static ScriptFactory Find(const std::string& name);
		// The name `type` was registered under, or "" if it wasn't.
		static const std::string& GetName(std::type_index type);
		// Every registered name, sorted.
		static std::vector<std::string> GetNames();
	};
}
