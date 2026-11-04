#include "hypch.h"
#include "SceneStack.h"

#include "Hydro/Core/Log.h"
#include "Hydro/Scene/GameSession.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SceneSerializer.h"

namespace Hydro
{
	void SceneStack::Start(const Ref<Scene>& scene)
	{
		Stop();

		SceneTransition::Clear();
		SceneTransition::SetDepth(0);
		SceneTransition::SetBelowName(std::string());

		// A new run: a fresh game session before the first scene's scripts exist.
		GameSession::Start();

		m_Active = scene;
		m_FirstScene = scene->GetName();
		m_Active->OnRuntimeStart(); // builds the physics worlds
	}

	void SceneStack::Stop()
	{
		if (m_Active == nullptr)
		{
			return;
		}

		DropScenes();
		SceneTransition::Clear();
		SceneTransition::SetDepth(0);
		SceneTransition::SetBelowName(std::string());

		// The run is over once its scenes (and their scripts' OnDestroy) are gone. A host that still holds a scene
		// releases it before Stop, so those scripts end while the session exists.
		GameSession::Stop();
	}

	SceneTransition::Type SceneStack::ProcessTransition(const std::filesystem::path& scenesDirectory, uint32_t viewportWidth,
		uint32_t viewportHeight)
	{
		const SceneTransition::Request request = SceneTransition::Take();
		if (request.Kind == SceneTransition::Type::None || m_Active == nullptr)
		{
			return SceneTransition::Type::None;
		}

		if (request.Kind == SceneTransition::Type::Quit)
		{
			return SceneTransition::Type::Quit;
		}

		if (request.Kind == SceneTransition::Type::Restart)
		{
			// A new run on a freshly read scene: the old run's scenes and session go first.
			const std::string name = request.SceneName.empty() ? m_FirstScene : request.SceneName;
			Ref<Scene> next = LoadScene(scenesDirectory, name);
			if (next == nullptr)
			{
				return SceneTransition::Type::None;
			}
			Start(next);
			m_Active->OnViewportResize(viewportWidth, viewportHeight);
			return request.Kind;
		}

		if (request.Kind == SceneTransition::Type::Push || request.Kind == SceneTransition::Type::Replace)
		{
			// Load into a fresh scene, and only switch when that worked.
			Ref<Scene> next = LoadScene(scenesDirectory, request.SceneName);
			if (next == nullptr)
			{
				return SceneTransition::Type::None;
			}
			if (request.Kind == SceneTransition::Type::Replace)
			{
				DropScenes();
			}
			else
			{
				m_Suspended.push_back(m_Active);
			}
			m_Active = next;
		}
		else
		{
			if (m_Suspended.empty())
			{
				HY_CORE_WARN("Scene pop: there is no scene below this one");
				return SceneTransition::Type::None;
			}

			// The top scene goes (its scripts' OnDestroy run), the one below carries on as it was.
			m_Active = m_Suspended.back();
			m_Suspended.pop_back();
		}

		m_Active->OnViewportResize(viewportWidth, viewportHeight);
		SceneTransition::SetDepth((uint32_t)m_Suspended.size());
		SceneTransition::SetBelowName(m_Suspended.empty() ? std::string() : m_Suspended.back()->GetName());
		return request.Kind;
	}

	Ref<Scene> SceneStack::LoadScene(const std::filesystem::path& scenesDirectory, const std::string& name)
	{
		const std::filesystem::path path = scenesDirectory / (name + SceneSerializer::FileExtension);
		Ref<Scene> scene = CreateRef<Scene>(name, false);
		SceneSerializer serializer(scene);
		if (serializer.Deserialize(path) == false)
		{
			HY_CORE_WARN("Scene switch: could not load {}", path.string());
			return nullptr;
		}
		scene->SetName(name);
		scene->Init();
		return scene;
	}

	// Destroying a scene runs its scripts' OnDestroy: the running one first, then the ones below it, the top one first.
	void SceneStack::DropScenes()
	{
		if (m_Active != nullptr)
		{
			m_Active->OnRuntimeStop();
			m_Active = nullptr;
		}
		while (m_Suspended.empty() == false)
		{
			m_Suspended.pop_back();
		}
	}

	std::vector<Ref<Scene>> SceneStack::GetScenes() const
	{
		std::vector<Ref<Scene>> scenes = m_Suspended;
		if (m_Active != nullptr)
		{
			scenes.push_back(m_Active);
		}
		return scenes;
	}
}
