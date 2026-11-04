#include "ComponentDrawers.h"

#include <Hydro/Scene/ComponentRegistry.h>

#include <algorithm>
#include <cstring>

namespace Hydro
{
	const std::vector<ComponentDrawer>& ComponentDrawers::GetAll()
	{
		static const std::vector<ComponentDrawer> s_Drawers = []()
		{
			std::vector<ComponentDrawer> drawers;
			AddSceneDrawers(drawers);
			AddSpriteDrawers(drawers);
			AddMeshDrawers(drawers);
			AddPhysicsDrawers(drawers);
			AddUIDrawers(drawers);

			// Every saved engine component should have a section (the parent is the hierarchy's).
			for (const EngineComponentInfo& type : ComponentRegistry::GetEngineTypes())
			{
				const bool drawn = std::any_of(drawers.begin(), drawers.end(),
					[&](const ComponentDrawer& drawer) { return drawer.CppType == type.CppType; });
				if (drawn == false && type.Save != nullptr)
				{
					HY_WARN("Inspector: engine component {} has no drawer (Inspector/<Family>ComponentDrawers.cpp)", type.Name);
				}
			}
			return drawers;
		}();
		return s_Drawers;
	}

	void ComponentDrawers::DrawAll(Entity entity, InspectorContext& context)
	{
		for (const ComponentDrawer& drawer : GetAll())
		{
			if (drawer.Has(entity))
			{
				drawer.Draw(entity, context);
			}
		}
		DrawGameComponents(entity, context);
		DrawScripts(entity, context);

		for (const std::function<void(Entity)>& remove : context.Removals)
		{
			remove(entity);
		}
		context.Removals.clear();
	}

	static void DrawAddMenuItem(const ComponentDrawer& drawer, Entity entity)
	{
		const bool canAdd = drawer.Has(entity) == false && (drawer.CanAdd == nullptr || drawer.CanAdd(entity));
		if (ImGui::MenuItem(drawer.MenuLabel, nullptr, false, canAdd))
		{
			drawer.Add(entity);
		}
	}

	void ComponentDrawers::DrawAddMenuItems(Entity entity)
	{
		const std::vector<ComponentDrawer>& drawers = GetAll();
		std::vector<const char*> menus;
		for (const ComponentDrawer& drawer : drawers)
		{
			if (drawer.Menu[0] == '\0')
			{
				DrawAddMenuItem(drawer, entity);
			}
			else if (std::find_if(menus.begin(), menus.end(), [&](const char* menu) { return std::strcmp(menu, drawer.Menu) == 0; })
				== menus.end())
			{
				menus.push_back(drawer.Menu);
			}
		}

		for (const char* menu : menus)
		{
			if (ImGui::BeginMenu(menu))
			{
				for (const ComponentDrawer& drawer : drawers)
				{
					if (std::strcmp(drawer.Menu, menu) == 0)
					{
						DrawAddMenuItem(drawer, entity);
					}
				}
				ImGui::EndMenu();
			}
		}

		DrawGameMenuItems(entity);
		if (ImGui::BeginMenu("Script"))
		{
			DrawScriptMenuItems(entity);
			ImGui::EndMenu();
		}
	}
}
