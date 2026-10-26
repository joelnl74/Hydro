#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace Hydro
{
	// How game code asks the program that runs the scenes (the editor's Play mode, later a standalone
	// runtime) to switch scenes while the game plays. A scene stack: Push loads another scene on top and
	// stops updating and drawing the one below it (that one is kept exactly as it was: entities, scripts,
	// physics bodies), Pop throws the top scene away and the one below carries on.
	//
	// Scripts only record the request; the host reads it with Take() after the frame's update (never in
	// the middle of it) and does the switch. At most one request is kept: the last one wins. A host that
	// does not support scenes ignores it.
	//
	// SceneName is a scene file name without folder or extension, looked up in the project's scenes folder
	// (Push("Battle") loads Assets/Scenes/Battle.hyscene).
	class SceneTransition
	{
	public:
		enum class Type : uint8_t { None, Push, Pop };

		struct Request
		{
			Type Kind = Type::None;
			std::string SceneName;
		};

		static void Push(const std::string& sceneName)
		{
			s_Request.Kind = Type::Push;
			s_Request.SceneName = sceneName;
		}

		static void Pop()
		{
			s_Request.Kind = Type::Pop;
			s_Request.SceneName.clear();
		}

		static bool HasRequest() { return s_Request.Kind != Type::None; }

		// The pending request (Type::None when there is none), which is cleared.
		static Request Take()
		{
			Request request = std::move(s_Request);
			s_Request = Request();
			return request;
		}

		// Forgets a pending request (the host calls it when the game starts or stops).
		static void Clear() { s_Request = Request(); }

		// Scenes below the running one on the stack: 0 for the first scene. Set by the host.
		static uint32_t GetDepth() { return s_Depth; }
		static void SetDepth(uint32_t depth) { s_Depth = depth; }

	private:
		// Defined after the class: g++ can't initialise an inline static of a nested type with default
		// member initialisers inside the class (MSVC can).
		static Request s_Request;
		static inline uint32_t s_Depth = 0;
	};

	inline SceneTransition::Request SceneTransition::s_Request;
}
