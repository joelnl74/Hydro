#pragma once
#include "Hydro.h"

#include <cmath>

// Small native scripts to try play mode with. They're registered in OxygenEditorApplication::OnInit,
// so they show up under Add Component > Script in the inspector and are saved with the scene.
namespace Hydro
{
	// Turns the entity around Z.
	class SpinScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			auto& transform = GetComponent<TransformComponent>();
			transform.Rotation.z += m_Speed * deltaTime;
		}

	private:
		float m_Speed = 1.5f; // radians per second
	};

	// Moves the entity up and down around where it started.
	class BobScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			m_StartY = GetComponent<TransformComponent>().Translation.y;
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;
			auto& transform = GetComponent<TransformComponent>();
			transform.Translation.y = m_StartY + std::sin(m_Time * 2.0f) * 0.5f;
		}

	private:
		float m_StartY = 0.0f;
		float m_Time = 0.0f;
	};

	inline void RegisterExampleScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<SpinScript>("Spin");
		ScriptRegistry::Register<BobScript>("Bob");
	}
}
