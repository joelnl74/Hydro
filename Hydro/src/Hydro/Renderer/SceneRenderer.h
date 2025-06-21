#pragma once

#include "Hydro/Scene/Scene.h"

namespace Hydro
{
	class SceneRenderer
	{
	public:
		SceneRenderer(Ref<Scene>& scene)
			: m_Scene(scene) {
		}
		
		void RenderScene();
		void RenderCompositeImage(float x, float y);

	private:
		Ref<Scene> m_Scene;
	};
}

