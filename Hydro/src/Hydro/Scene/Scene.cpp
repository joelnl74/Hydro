#include <hypch.h>
#include "Scene.h"
#include "../Renderer/Renderer2D.h"

namespace Hydro
{
	Scene::Scene(const std::string& debugName, bool isEditorScene)
	{

	}

	Scene::~Scene()
	{
	}
	
	void Scene::Init()
	{
	}
	
	void Scene::OnUpdate()
	{
	}
	
	void Scene::OnRender()
	{
		for (int i = 0; i < m_spriteComponents.size(); i++)
		{
			Renderer2D::DrawQuad(m_spriteComponents[i].Position, m_spriteComponents[i].Color);
		}
	}
}
