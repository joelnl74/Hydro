#pragma once
#include <string>
#include <vector>

#include "Components/SpriteComponent.h"

namespace Hydro
{
	class Scene
	{
	public:
		Scene(const std::string& debugName = "Scene", bool isEditorScene = false);
		~Scene();

		void Init();

		void OnUpdate();
		void OnRender();

	public:
		// TODO: Get components from components pools.
		std::vector<SpriteComponent*> m_spriteComponents;
	};
}
