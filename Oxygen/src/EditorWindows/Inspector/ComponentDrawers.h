#pragma once
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/Components/Components.h>

#include <imgui/imgui.h>

#include <functional>
#include <typeindex>
#include <vector>

namespace Hydro
{
	class MaterialEditor;
	class TerrainTool;

	// What the component drawers need from the inspector while it draws one entity.
	struct InspectorContext
	{
		TerrainTool* Terrain = nullptr;   // may be null
		MaterialEditor* Materials = nullptr;
		// While playing, some edits (painting a terrain asset shared with the editor scene) wait for Stop.
		bool Playing = false;
		// Requested while drawing, applied after it, when no component reference is held.
		std::vector<std::function<void(Entity)>> Removals;
	};

	// One engine component's inspector section and Add Component menu item.
	struct ComponentDrawer
	{
		const char* Name = "";
		std::type_index CppType = typeid(void);
		// The Add Component submenu ("" = the top level) and the item's label there.
		const char* Menu = "";
		const char* MenuLabel = "";
		bool (*Has)(const Entity& entity) = nullptr;
		void (*Draw)(Entity entity, InspectorContext& context) = nullptr;
		// Adds it with what it needs to work (a transform to place it, a rect for a UI element).
		void (*Add)(Entity entity) = nullptr;
		// Null: it can be added whenever the entity hasn't one.
		bool (*CanAdd)(const Entity& entity) = nullptr;
	};

	// The inspector's component sections, one drawer per engine component, then the game's components and the
	// scripts. A new engine component gets a drawer in its family's file (SpriteComponentDrawers.cpp, ...).
	class ComponentDrawers
	{
	public:
		using RemoveFunction = void (*)(Entity entity);

		// In inspector order: scene, sprite, mesh, physics, then UI drawers.
		static const std::vector<ComponentDrawer>& GetAll();

		// Every section the entity has, then the requested removals.
		static void DrawAll(Entity entity, InspectorContext& context);
		// The Add Component popup's items: top-level drawers, their submenus, Game and Script.
		static void DrawAddMenuItems(Entity entity);

		// A collapsible section for one component type; right-click the header to remove it. The removal (`remove`, or
		// RemoveComponent<T>) is only requested here.
		template<typename T, typename UIFunction>
		static void Section(const char* name, Entity entity, InspectorContext& context, UIFunction uiFunction,
			RemoveFunction remove = nullptr)
		{
			if (entity.HasComponent<T>() == false)
			{
				return;
			}

			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

			ImGui::PushID(name);
			ImGui::Spacing();
			bool open = ImGui::TreeNodeEx("##component", flags, "%s", name);

			if (ImGui::BeginPopupContextItem("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					context.Removals.push_back(remove != nullptr ? remove : &Remove<T>);
				}

				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(entity.GetComponent<T>());
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		template<typename T>
		static ComponentDrawer Make(const char* name, void (*draw)(Entity, InspectorContext&), void (*add)(Entity),
			const char* menu = "", const char* menuLabel = nullptr)
		{
			ComponentDrawer drawer;
			drawer.Name = name;
			drawer.CppType = typeid(T);
			drawer.Menu = menu;
			drawer.MenuLabel = menuLabel != nullptr ? menuLabel : name;
			drawer.Has = &Has<T>;
			drawer.Draw = draw;
			drawer.Add = add;
			return drawer;
		}

		template<typename T>
		static bool Has(const Entity& entity)
		{
			return entity.HasComponent<T>();
		}

		template<typename T>
		static void Remove(Entity entity)
		{
			entity.RemoveComponent<T>();
		}

		template<typename T>
		static void AddOnly(Entity entity)
		{
			entity.AddComponent<T>();
		}

		// Placed by the entity's transform (a sprite needs one to be drawn, a body to be placed): one comes with it.
		template<typename T>
		static void AddWithTransform(Entity entity)
		{
			entity.AddComponent<T>();
			if (entity.HasComponent<TransformComponent>() == false)
			{
				entity.AddComponent<TransformComponent>();
			}
		}

		// UI elements are placed by a rect: every visual gets one with it. A canvas needs none.
		template<typename T>
		static void AddWithRect(Entity entity)
		{
			entity.AddComponent<T>();
			if (entity.HasComponent<UIRectComponent>() == false)
			{
				entity.AddComponent<UIRectComponent>();
			}
		}

		// One menu item per registered script; picking one binds it to the entity.
		static void DrawScriptMenuItems(Entity entity);

	private:
		static void AddSceneDrawers(std::vector<ComponentDrawer>& drawers);
		static void AddSpriteDrawers(std::vector<ComponentDrawer>& drawers);
		static void AddMeshDrawers(std::vector<ComponentDrawer>& drawers);
		static void AddPhysicsDrawers(std::vector<ComponentDrawer>& drawers);
		static void AddUIDrawers(std::vector<ComponentDrawer>& drawers);

		// The game's components (HY_COMPONENT), their fields drawn as data.
		static void DrawGameComponents(Entity entity, InspectorContext& context);
		static void DrawGameMenuItems(Entity entity);
		static void DrawScripts(Entity entity, InspectorContext& context);
	};
}
