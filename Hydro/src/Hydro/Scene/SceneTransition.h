#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace Hydro
{
	// How game code asks the host (Play mode, the Runtime) to switch scenes, restart the run or quit. The host takes the
	// last request after the frame's update. Names are scene file names ("Battle").
	class SceneTransition
	{
	public:
		enum class Type : uint8_t { None, Push, Pop, Replace, Restart, Quit };

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

		// Drops every scene of the run and loads `sceneName` as its only one; the game session stays (a loaded save).
		static void Replace(const std::string& sceneName)
		{
			s_Request.Kind = Type::Replace;
			s_Request.SceneName = sceneName;
		}

		// Ends the run and starts a new one, with a fresh game session, on `sceneName`; empty: the scene the run
		// started in.
		static void Restart(const std::string& sceneName = std::string())
		{
			s_Request.Kind = Type::Restart;
			s_Request.SceneName = sceneName;
		}

		static void Quit()
		{
			s_Request.Kind = Type::Quit;
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

		// The scene right below the running one (what Pop returns to); empty for the first scene. Set by the host.
		static const std::string& GetBelowName() { return s_BelowName; }
		static void SetBelowName(const std::string& name) { s_BelowName = name; }

	private:
		// Defined after the class: g++ can't initialise an inline static of a nested type with default member
		// initialisers inside the class (MSVC can).
		static Request s_Request;
		static inline uint32_t s_Depth = 0;
		static inline std::string s_BelowName;
	};

	inline SceneTransition::Request SceneTransition::s_Request;
}
