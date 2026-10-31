#include "hypch.h"
#include "SceneStack.h"

#include "Hydro/Core/Log.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SceneSerializer.h"

namespace Hydro
{
	void SceneStack::Start(const Ref<Scene>& scene)
	{
		Stop();

		SceneTransition::Clear();
		SceneTransition::SetDepth(0);

		m_Active = scene;
		m_Active->OnRuntimeStart(); // builds the physics worlds
	}

	void SceneStack::Stop()
	{
		if (m_Active == nullptr)
		{
			return;
		}

		// Destroying a scene runs its scripts' OnDestroy: the running one first, then the ones below it,
		// the top one first.
		m_Active->OnRuntimeStop();
		m_Active = nullptr;
		while (m_Suspended.empty() == false)
		{
			m_Suspended.pop_back();
		}

		SceneTransition::Clear();
		SceneTransition::SetDepth(0);
	}

	SceneTransition::Type SceneStack::ProcessTransition(const std::filesystem::path& scenesDirectory, uint32_t viewportWidth, uint32_t viewportHeight)
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

		if (request.Kind == SceneTransition::Type::Push)
		{
			// Load into a fresh scene, and only switch when that worked.
			const std::filesystem::path path = scenesDirectory / (request.SceneName + SceneSerializer::FileExtension);
			Ref<Scene> next = CreateRef<Scene>(request.SceneName, false);
			SceneSerializer serializer(next);
			if (serializer.Deserialize(path) == false)
			{
				HY_CORE_WARN("Scene push: could not load {}", path.string());
				return SceneTransition::Type::None;
			}

			next->SetName(request.SceneName);
			next->Init();

			m_Suspended.push_back(m_Active);
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
		return request.Kind;
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
