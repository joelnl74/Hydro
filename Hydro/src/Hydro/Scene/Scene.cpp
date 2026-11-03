#include <hypch.h>
#include "Scene.h"
#include "Entity.h"
#include "EntityReferences.h"
#include "ComponentRegistry.h"
#include "Scripting/ScriptableEntity.h"
#include "Scripting/ScriptSystem.h"
#include "SceneGraph.h"
#include "SceneTransforms.h"
#include "SpriteAnimator.h"
#include "SystemContext.h"
#include "Hydro/Physics/PhysicsScene.h"
#include "Hydro/Physics/PhysicsScene3D.h"
#include "ParticleScene.h"
#include "UI/UILayout.h"
#include "UI/UIScene.h"
#include "Hydro/Renderer/Camera.h"

#include "../Renderer/Renderer2D.h"
#include "Hydro/Asset/AssetManager.h"
#include <Hydro/Core/Time.h>
#include <Hydro/Core/UUID.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
namespace Hydro
{
	// UIResolvedRectComponent comes and goes with UIRectComponent (registry signals, connected in the constructor), so
	// ResolveUILayout only writes values.
	static void AddResolvedRect(entt::registry& registry, entt::entity entity)
	{
		registry.emplace_or_replace<UIResolvedRectComponent>(entity);
	}

	static void RemoveResolvedRect(entt::registry& registry, entt::entity entity)
	{
		registry.remove_if_exists<UIResolvedRectComponent>(entity);
	}

	Scene::Scene(const std::string& debugName, [[maybe_unused]] bool isEditorScene)
		: m_Name(debugName)
	{
		m_Registry.on_construct<UIRectComponent>().connect<&AddResolvedRect>();
		m_Registry.on_destroy<UIRectComponent>().connect<&RemoveResolvedRect>();
	}

	Scene::~Scene()
	{
		// Scripts clean up while the registry still exists. The open stage queues (and then drops) whatever their
		// OnDestroy asks for, so the registry doesn't change under this loop.
		m_StageDepth++;

		auto view = m_Registry.view<NativeScriptComponent>();
		for (auto entityHandle : view)
		{
			auto& scriptComponent = view.get<NativeScriptComponent>(entityHandle);
			for (auto& script : scriptComponent.Scripts)
			{
				if (script.Instance != nullptr)
				{
					script.Instance->OnDestroy();
				}
			}
		}

		m_Deferred.clear();
		m_PendingDestroy.clear();
		m_Particles.reset();
		m_Physics3D.reset();
		m_Physics.reset();
	}

	void Scene::Init()
	{

	}

	template<typename... Component>
	static void CopyComponents(ComponentGroup<Component...>, entt::registry& destination, const entt::registry& source,
		const std::unordered_map<entt::entity, entt::entity>& entityMap)
	{
		([&]()
		{
			auto view = source.view<const Component>();
			for (auto sourceEntity : view)
			{
				auto it = entityMap.find(sourceEntity);
				if (it != entityMap.end())
				{
					destination.emplace_or_replace<Component>(it->second, view.template get<const Component>(sourceEntity));
				}
			}
		}(), ...);
	}

	Ref<RenderProfile> Scene::GetRenderProfile() const
	{
		return (uint64_t)m_Look.Profile != 0 ? AssetManager::GetLoadedAsset<RenderProfile>(m_Look.Profile) : nullptr;
	}

	SceneEnvironment Scene::GetActiveEnvironment() const
	{
		if (m_Look.Environment == RenderLookSource::Profile)
		{
			if (const Ref<RenderProfile> profile = GetRenderProfile(); profile != nullptr)
			{
				return profile->GetData().Environment;
			}
		}
		return m_Environment;
	}

	ScenePostProcess Scene::GetActivePostProcess() const
	{
		if (m_Look.PostProcess == RenderLookSource::Profile)
		{
			if (const Ref<RenderProfile> profile = GetRenderProfile(); profile != nullptr)
			{
				return profile->GetData().PostProcess;
			}
		}
		return m_PostProcess;
	}

	Ref<Scene> Scene::Copy(const Ref<Scene>& source)
	{
		Ref<Scene> copy = CreateRef<Scene>(source->GetName());
		copy->m_ViewportWidth = source->m_ViewportWidth;
		copy->m_ViewportHeight = source->m_ViewportHeight;
		copy->m_RenderSettings = source->m_RenderSettings;
		copy->m_Environment = source->m_Environment;
		copy->m_PostProcess = source->m_PostProcess;
		copy->m_Look = source->m_Look;

		const entt::registry& sourceRegistry = source->m_Registry;

		// Created in the source's list order, so the copy lists entities the same way.
		std::unordered_map<entt::entity, entt::entity> entityMap;
		for (entt::entity sourceEntity : source->GetEntityOrder())
		{
			const UUID uuid = sourceRegistry.get<IDComponent>(sourceEntity).Id;
			std::string name;
			if (sourceRegistry.has<TagComponent>(sourceEntity))
			{
				name = sourceRegistry.get<TagComponent>(sourceEntity).Tag;
			}

			entityMap[sourceEntity] = copy->CreateEntityWithUUID(uuid, name);
		}

		CopyComponents(CopyableComponents{}, copy->m_Registry, sourceRegistry, entityMap);
		// The game's own components (HY_COMPONENT, ComponentRegistry).
		for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
		{
			type.CopyAll(copy->m_Registry, sourceRegistry, entityMap);
		}

		// Native scripts: copy the bindings, not the instances (those belong to the source scene).
		auto scripts = sourceRegistry.view<const NativeScriptComponent>();
		for (auto sourceEntity : scripts)
		{
			auto it = entityMap.find(sourceEntity);
			if (it == entityMap.end())
			{
				continue;
			}

			NativeScriptComponent& copied = copy->m_Registry.emplace<NativeScriptComponent>(it->second);
			for (const auto& script : scripts.get<const NativeScriptComponent>(sourceEntity).Scripts)
			{
				NativeScriptComponent::Script binding;
				binding.Name = script.Name;
				binding.Instantiate = script.Instantiate;
				binding.Properties = script.Properties;
				binding.KeptProperties = script.KeptProperties;
				copied.Scripts.push_back(std::move(binding));
			}
		}

		return copy;
	}

	template<typename... Component>
	static void CopyComponentsOfEntity(ComponentGroup<Component...>, entt::registry& registry, entt::entity source,
		entt::entity destination)
	{
		([&]()
		{
			if (registry.has<Component>(source))
			{
				// Copy first: emplacing can reallocate the pool the source component lives in.
				Component copy = registry.get<Component>(source);
				registry.emplace_or_replace<Component>(destination, std::move(copy));
			}
		}(), ...);
	}

	// Native scripts: copies the bindings, not the instances (those belong to the source entity).
	static void CopyScriptBindings(entt::registry& registry, entt::entity source, entt::entity destination)
	{
		if (registry.has<NativeScriptComponent>(source) == false)
		{
			return;
		}

		std::vector<NativeScriptComponent::Script> bindings;
		for (const auto& script : registry.get<NativeScriptComponent>(source).Scripts)
		{
			NativeScriptComponent::Script binding;
			binding.Name = script.Name;
			binding.Instantiate = script.Instantiate;
			binding.Properties = script.Properties; // HY_PROPERTY values (#141)
			binding.KeptProperties = script.KeptProperties; // saved values no property reads (#143)
			bindings.push_back(std::move(binding));
		}
		registry.emplace_or_replace<NativeScriptComponent>(destination).Scripts = std::move(bindings);
	}

	void Scene::RemapEntityReferences(const std::vector<Entity>& entities, const std::unordered_map<UUID, UUID>& remap)
	{
		for (Entity entity : entities)
		{
			if (m_Registry.valid(entity) == false)
			{
				continue;
			}
			EntityReferences::ForEach(entity, [&](UUID& id, const EntityReference& reference)
			{
				const auto found = remap.find(id);
				if (found == remap.end() || found->second == id)
				{
					return;
				}
				id = found->second;
				if (reference.Kind == EntityReferenceKind::Parent)
				{
					m_Graph.MarkDirty();
				}
			});
		}
	}

	Entity Scene::FindEntityByTag(const std::string& tag)
	{
		for (const entt::entity handle : GetEntityOrder())
		{
			if (m_Registry.valid(handle) && m_Registry.has<TagComponent>(handle) && m_Registry.get<TagComponent>(handle).Tag == tag)
			{
				return Entity{ handle, this };
			}
		}
		return {};
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false || m_Registry.has<IDComponent>(entity) == false)
		{
			return {};
		}

		std::vector<Entity> originals;
		GetSubtree(entity, originals);
		if (originals.empty())
		{
			return {};
		}

		// The copies go to the end of the list in the originals' order, so they form one subtree there.
		std::vector<Entity> copies;
		std::unordered_map<UUID, UUID> remap;
		copies.reserve(originals.size());
		for (Entity original : originals)
		{
			Entity copy = CreateEntity(original.GetName());
			CopyComponentsOfEntity(CopyableComponents{}, m_Registry, original, copy);
			for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
			{
				type.CopyOne(m_Registry, original, copy);
			}
			CopyScriptBindings(m_Registry, original, copy);
			remap[original.GetUUID()] = copy.GetUUID();
			copies.push_back(copy);
		}
		RemapEntityReferences(copies, remap);
		m_Graph.MarkDirty();

		// Right after the original's subtree. The copy of `entity` kept its parent, so it is a sibling there.
		const int32_t originalIndex = m_Graph.GetIndex(entity);
		const int32_t copyIndex = m_Graph.GetIndex((entt::entity)copies.front());
		m_Graph.MoveSubtree((size_t)copyIndex, m_Graph.GetLayout().GetSiblingInsertIndex((size_t)originalIndex, true));
		m_Graph.CheckAfterEdit();

		return copies.front();
	}

	// ---- Hierarchy and world transforms: SceneGraph and SceneTransforms do the work --------------------

	const std::vector<entt::entity>& Scene::GetEntityOrder()
	{
		return m_Graph.GetOrder();
	}

	void Scene::MoveEntity(Entity entity, Entity target, bool placeAfter)
	{
		m_Graph.MoveEntity(entity, target, placeAfter);
	}

	bool Scene::SetParent(Entity child, Entity parent, bool keepWorldTransform)
	{
		return m_Graph.SetParent(child, parent, keepWorldTransform);
	}

	Entity Scene::GetParent(Entity entity)
	{
		return m_Graph.GetParent(entity);
	}

	void Scene::GetChildren(Entity entity, std::vector<Entity>& children)
	{
		m_Graph.GetChildren(entity, children);
	}

	bool Scene::HasChildren(Entity entity)
	{
		return m_Graph.HasChildren(entity);
	}

	void Scene::GetSubtree(Entity entity, std::vector<Entity>& subtree)
	{
		m_Graph.GetSubtree(entity, subtree);
	}

	bool Scene::IsAncestorOf(Entity ancestor, Entity entity)
	{
		return m_Graph.IsAncestorOf(ancestor, entity);
	}

	uint32_t Scene::GetDepth(Entity entity)
	{
		return m_Graph.GetDepth(entity);
	}

	void Scene::RepairHierarchy()
	{
		m_Graph.Repair();
	}

	bool Scene::ValidateHierarchy()
	{
		return m_Graph.Validate();
	}

	void Scene::ResolveTransforms()
	{
		m_Transforms.Resolve();
	}

	glm::mat4 Scene::GetWorldTransform(Entity entity)
	{
		return m_Transforms.GetWorld(entity);
	}

	bool Scene::SetWorldTransform(Entity entity, const glm::mat4& world)
	{
		return m_Transforms.SetWorld(entity, world);
	}

	glm::vec3 Scene::GetWorldPosition(Entity entity)
	{
		return m_Transforms.GetWorldPosition(entity);
	}

	bool Scene::SetWorldPosition(Entity entity, const glm::vec3& position)
	{
		return m_Transforms.SetWorldPosition(entity, position);
	}

	void Scene::ResolveUILayout()
	{
		m_Graph.Ensure();

		const glm::vec2 viewSize{ (float)m_ViewportWidth, (float)m_ViewportHeight };
		const size_t count = m_Graph.GetList().size();
		m_UICanvasMetrics.clear();
		m_UIWorldToViewState = 0;
		m_UILayout.resize(count);
		// Rects a layout group placed for its children, written when the group is reached (before its children).
		m_UIDriven.assign(count, UIDrivenRect{});
		size_t written = 0;
		for (size_t i = 0; i < count; i++)
		{
			const entt::entity handle = m_Graph.GetList()[i];
			const int32_t parent = m_Graph.GetLayout().GetParentIndex(i);
			const UILayoutEntry inherited = parent >= 0 ? m_UILayout[(size_t)parent] : UILayoutEntry{};
			UILayoutEntry& entry = m_UILayout[i];
			entry = inherited; // an entity without a rect passes its parent's on

			if (m_Registry.has<UICanvasComponent>(handle))
			{
				if (inherited.InCanvas)
				{
					// Nested canvases aren't supported: this one and everything on it is left out.
					if (m_NestedCanvasWarned.insert(handle).second)
					{
						const std::string name = m_Registry.has<TagComponent>(handle) ? m_Registry.get<TagComponent>(handle).Tag
							: std::string("?");
						HY_CORE_WARN("UI: canvas '{}' is inside another canvas; nested canvases aren't supported, it isn't drawn", name);
					}
					entry.Canvas = 0;
					entry.Visible = false;
				}
				else
				{
					const UICanvasComponent& canvas = m_Registry.get<UICanvasComponent>(handle);
					UICanvasMetrics metrics;
					const bool placed = PlaceUICanvas(i, canvas, viewSize, metrics);
					entry.Min = glm::vec2(0.0f);
					entry.Max = metrics.Size;
					entry.Visible = canvas.Visible && placed;
					entry.Canvas = m_Registry.has<IDComponent>(handle) ? m_Registry.get<IDComponent>(handle).Id : UUID(0);
					if (entry.Visible && (uint64_t)entry.Canvas != 0)
					{
						m_UICanvasMetrics[(uint64_t)entry.Canvas] = metrics;
					}
				}
				entry.InCanvas = true;

				// A rect on the canvas itself is the whole canvas.
				if (m_Registry.has<UIRectComponent>(handle))
				{
					UIResolvedRectComponent& resolved = m_Registry.get<UIResolvedRectComponent>(handle);
					resolved.Min = entry.Min;
					resolved.Max = entry.Max;
					resolved.Visible = entry.Visible && m_Registry.get<UIRectComponent>(handle).Visible;
					resolved.Canvas = entry.Canvas;
					resolved.ClipMin = glm::vec2(-1.0e9f);
					resolved.ClipMax = glm::vec2(1.0e9f);
					written++;
				}
				continue;
			}

			if (m_Registry.has<UIRectComponent>(handle) == false)
			{
				continue;
			}

			HY_CORE_ASSERT(m_Registry.has<UIResolvedRectComponent>(handle),
				"Scene: an entity has a UIRectComponent but no UIResolvedRectComponent");
			const UIRectComponent& rect = m_Registry.get<UIRectComponent>(handle);
			UIResolvedRectComponent& resolved = m_Registry.get<UIResolvedRectComponent>(handle);
			if ((uint64_t)inherited.Canvas == 0)
			{
				// No canvas above it: laid out as nothing, never drawn.
				resolved = UIResolvedRectComponent{};
				entry.Visible = false;
			}
			else
			{
				if (m_UIDriven[i].Driven)
				{
					entry.Min = m_UIDriven[i].Min;
					entry.Max = m_UIDriven[i].Max;
				}
				else
				{
					UILayout::ResolveRect(rect, inherited.Min, inherited.Max, entry.Min, entry.Max);
				}
				entry.Visible = inherited.Visible && rect.Visible;
				if (m_Registry.has<UILayoutGroupComponent>(handle))
				{
					ArrangeUIChildren(i, rect, m_Registry.get<UILayoutGroupComponent>(handle), entry.Min, entry.Max);
				}
				resolved.Min = entry.Min;
				resolved.Max = entry.Max;
				resolved.Visible = entry.Visible;
				resolved.Canvas = entry.Canvas;
				resolved.ClipMin = glm::vec2(-1.0e9f);
				resolved.ClipMax = glm::vec2(1.0e9f);
			}
			written++;
		}

		// Scroll views, outer ones first (list order): each moves and clips everything below it.
		if (m_Registry.size<UIScrollViewComponent>() > 0)
		{
			for (size_t i = 0; i < count; i++)
			{
				const entt::entity handle = m_Graph.GetList()[i];
				if (m_Registry.has<UIScrollViewComponent>(handle) && m_Registry.has<UIResolvedRectComponent>(handle))
				{
					ScrollUIView(i);
				}
			}
		}

		// Elements outside the list (raw registry entities without an IDComponent) have no canvas.
		if (written < m_Registry.size<UIRectComponent>())
		{
			auto view = m_Registry.view<UIRectComponent, UIResolvedRectComponent>();
			for (entt::entity handle : view)
			{
				if (m_Graph.IsListed(handle) == false)
				{
					view.get<UIResolvedRectComponent>(handle) = UIResolvedRectComponent{};
				}
			}
		}
	}

	bool Scene::GetUICanvasMetrics(UUID canvas, UICanvasMetrics& metrics) const
	{
		const auto found = m_UICanvasMetrics.find((uint64_t)canvas);
		if (found == m_UICanvasMetrics.end())
		{
			return false;
		}
		metrics = found->second;
		return true;
	}

	bool Scene::PlaceUICanvas(size_t index, const UICanvasComponent& canvas, const glm::vec2& viewSize, UICanvasMetrics& metrics)
	{
		metrics = UILayout::GetCanvasMetrics(canvas, viewSize);
		if (canvas.Space != UICanvasSpace::World)
		{
			return true;
		}

		// World: a WorldSize box at the canvas scale, its pivot on where the primary camera sees the entity.
		metrics.Size = glm::max(canvas.WorldSize, glm::vec2(1.0f));
		if (m_UIWorldToViewState == 0)
		{
			m_UIWorldToViewState = -1;
			Entity camera = GetPrimaryCameraEntity();
			if ((bool)camera && camera.HasComponent<CameraComponent>())
			{
				// As SceneRenderer::RenderRuntime looks: the camera's position and rotation, its projection at the view's
				// aspect; then back from clip space to view pixels through the screen overlay's own projection.
				glm::mat4 cameraTransform(1.0f);
				if (camera.HasComponent<WorldTransformComponent>())
				{
					const WorldTransformComponent& transform = camera.GetComponent<WorldTransformComponent>();
					cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(transform.Rotation);
				}
				const glm::mat4 projection = camera.GetComponent<CameraComponent>().Camera.GetProjection(GetViewportAspectRatio());
				const glm::mat4 screen = Camera::Orthographic(0.0f, viewSize.x, 0.0f, viewSize.y, -1.0f, 1.0f);
				m_UIWorldToView = glm::inverse(screen) * projection * glm::inverse(cameraTransform);
				m_UIWorldToViewState = 1;
			}
		}
		if (m_UIWorldToViewState < 0)
		{
			return false;
		}

		// The entity's world position (a canvas without a transform of its own sits where its parent is).
		const std::vector<glm::mat4>& resolved = m_Transforms.GetResolvedMatrices();
		const glm::mat4 world = resolved.size() == m_Graph.GetList().size() ? resolved[index]
			: GetWorldTransform(Entity{ m_Graph.GetList()[index], this });
		const glm::vec4 clip = m_UIWorldToView * glm::vec4(glm::vec3(world[3]), 1.0f);
		if (clip.w <= 1.0e-5f)
		{
			return false; // behind the camera
		}
		const glm::vec2 point = glm::vec2(clip) / clip.w;
		metrics.Offset = glm::round(point - canvas.WorldPivot * metrics.Size * metrics.Scale);
		return true;
	}

	void Scene::ScrollUIView(size_t index)
	{
		const entt::entity handle = m_Graph.GetList()[index];
		UIScrollViewComponent& scroll = m_Registry.get<UIScrollViewComponent>(handle);
		const UIResolvedRectComponent view = m_Registry.get<UIResolvedRectComponent>(handle);
		if ((uint64_t)view.Canvas == 0)
		{
			return;
		}

		// The content: the visible UI children's rects together, as laid out (unscrolled).
		m_Graph.GetLayout().GetChildIndices(index, m_UIChildIndices);
		bool any = false;
		float contentBottom = 0.0f;
		float contentTop = 0.0f;
		for (size_t child : m_UIChildIndices)
		{
			const entt::entity childHandle = m_Graph.GetList()[child];
			if (m_Registry.has<UIResolvedRectComponent>(childHandle) == false || m_Registry.has<UICanvasComponent>(childHandle))
			{
				continue;
			}
			const UIResolvedRectComponent& rect = m_Registry.get<UIResolvedRectComponent>(childHandle);
			if (m_Registry.get<UIRectComponent>(childHandle).Visible == false)
			{
				continue;
			}
			contentBottom = any ? std::min(contentBottom, rect.Min.y) : rect.Min.y;
			contentTop = any ? std::max(contentTop, rect.Max.y) : rect.Max.y;
			any = true;
		}
		// Moving the content up by s shows its bottom once contentBottom + s reaches the view's bottom.
		const float maxScroll = any ? std::max(0.0f, std::round(view.Min.y - contentBottom)) : 0.0f;
		float position = scroll.ScrollPosition;

		// The focused button inside the view stays whole in view.
		if (scroll.FollowFocus && m_UI != nullptr)
		{
			const Entity focused = m_UI->GetFocused(*this);
			if ((bool)focused && focused.HasComponent<UIResolvedRectComponent>())
			{
				const int32_t focusedIndex = m_Graph.GetIndex((entt::entity)focused);
				const UIResolvedRectComponent& rect = focused.GetComponent<UIResolvedRectComponent>();
				if (focusedIndex >= 0 && m_Graph.GetLayout().IsAncestor(index, (size_t)focusedIndex) && rect.Visible)
				{
					if (rect.Max.y + position > view.Max.y)
					{
						position = view.Max.y - rect.Max.y;
					}
					if (rect.Min.y + position < view.Min.y)
					{
						position = view.Min.y - rect.Min.y;
					}
				}
			}
		}
		position = std::round(std::clamp(position, 0.0f, maxScroll));
		scroll.ScrollPosition = position;

		// Everything below the view moves up by it and is cut to the view.
		const size_t end = m_Graph.GetLayout().GetSubtreeEnd(index);
		for (size_t i = index + 1; i < end; i++)
		{
			const entt::entity element = m_Graph.GetList()[i];
			if (m_Registry.has<UIResolvedRectComponent>(element) == false)
			{
				continue;
			}
			UIResolvedRectComponent& rect = m_Registry.get<UIResolvedRectComponent>(element);
			if ((uint64_t)rect.Canvas != (uint64_t)view.Canvas)
			{
				continue;
			}
			rect.Min.y += position;
			rect.Max.y += position;
			rect.ClipMin = glm::max(rect.ClipMin, glm::max(view.Min, view.ClipMin));
			rect.ClipMax = glm::min(rect.ClipMax, glm::min(view.Max, view.ClipMax));
		}

		// The scrollbar's thumb, in its parent's rect (the track): as tall as the part in view, as far down as the scroll.
		Entity thumb = (uint64_t)scroll.Scrollbar != 0 ? GetEntityByUUID(scroll.Scrollbar) : Entity{};
		if ((bool)thumb == false || thumb.HasComponent<UIResolvedRectComponent>() == false)
		{
			return;
		}
		UIResolvedRectComponent& thumbRect = thumb.GetComponent<UIResolvedRectComponent>();
		const Entity track = GetParent(thumb);
		if (maxScroll <= 0.0f || (bool)track == false || track.HasComponent<UIResolvedRectComponent>() == false)
		{
			thumbRect.Visible = false;
			return;
		}
		const UIResolvedRectComponent& trackRect = track.GetComponent<UIResolvedRectComponent>();
		const float trackHeight = trackRect.Max.y - trackRect.Min.y;
		const float viewHeight = view.Max.y - view.Min.y;
		const float thumbHeight = std::round(std::clamp(trackHeight * viewHeight / std::max(viewHeight + maxScroll, 1.0f),
			std::min(8.0f, trackHeight), trackHeight));
		const float top = trackRect.Max.y - std::round((trackHeight - thumbHeight) * (position / maxScroll));
		thumbRect.Max.y = top;
		thumbRect.Min.y = top - thumbHeight;
	}

	void Scene::ArrangeUIChildren(size_t index, const UIRectComponent& rect, const UILayoutGroupComponent& group, glm::vec2& min,
		glm::vec2& max)
	{
		// The visible UI children (not nested canvases), at the sizes their own rects give them in the group's rect.
		m_Graph.GetLayout().GetChildIndices(index, m_UIChildIndices);
		m_UIItems.clear();
		m_UIItemIndices.clear();
		for (size_t child : m_UIChildIndices)
		{
			const entt::entity handle = m_Graph.GetList()[child];
			if (m_Registry.has<UIRectComponent>(handle) == false || m_Registry.has<UICanvasComponent>(handle))
			{
				continue;
			}
			const UIRectComponent& childRect = m_Registry.get<UIRectComponent>(handle);
			if (childRect.Visible == false)
			{
				continue; // hidden: no room, the gap closes
			}
			UILayoutItem item;
			glm::vec2 childMin;
			glm::vec2 childMax;
			UILayout::ResolveRect(childRect, min, max, childMin, childMax);
			item.Size = childMax - childMin;
			m_UIItems.push_back(item);
			m_UIItemIndices.push_back(child);
		}

		UILayout::ArrangeGroup(group, rect.Pivot, min, max, m_UIItems);
		for (size_t k = 0; k < m_UIItems.size(); k++)
		{
			UIDrivenRect& driven = m_UIDriven[m_UIItemIndices[k]];
			driven.Driven = true;
			driven.Min = m_UIItems[k].Min;
			driven.Max = m_UIItems[k].Max;
		}
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID({}, name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		// An entity with this UUID is already here (an entry for a destroyed one doesn't count).
		const auto existing = m_EntityMap.find(uuid);
		if (existing != m_EntityMap.end())
		{
			const entt::entity handle = existing->second;
			if (m_Registry.valid(handle) && m_Registry.has<IDComponent>(handle) && m_Registry.get<IDComponent>(handle).Id == uuid)
			{
				HY_CORE_ERROR("Scene '{0}': UUID {1} is already used by an entity; '{2}' gets a new one", m_Name, (uint64_t)uuid, name);
				do
				{
					uuid = UUID();
				}
				while (m_EntityMap.find(uuid) != m_EntityMap.end());
			}
		}

		Entity entity = { m_Registry.create(), this };
		auto& tag = entity.AddComponent<TagComponent>();
		entity.AddComponent<IDComponent>(uuid);
		
		tag.Tag = name.empty() ? "Entity" : name;

		m_EntityMap[uuid] = (entt::entity)entity;
		m_Graph.Add((entt::entity)entity);

		return entity;
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		for (int attempt = 0; attempt < 2; attempt++)
		{
			const auto found = m_EntityMap.find(uuid);
			if (found != m_EntityMap.end())
			{
				const entt::entity handle = found->second;
				if (m_Registry.valid(handle) && m_Registry.has<IDComponent>(handle) && m_Registry.get<IDComponent>(handle).Id == uuid)
				{
					return Entity{ handle, this };
				}

				// Destroyed (or its ID changed) behind the scene's back.
				m_EntityMap.erase(found);
			}

			// A miss: an entity may have got an IDComponent without CreateEntity, so check the registry once before saying no.
			if (attempt == 0)
			{
				if (m_EntityMap.size() == m_Registry.size<IDComponent>())
				{
					break;
				}

				RebuildEntityMap();
			}
		}

		return {};
	}

	void Scene::RebuildEntityMap()
	{
		m_EntityMap.clear();
		for (entt::entity handle : m_Registry.view<IDComponent>())
		{
			// The first entity with a UUID keeps it if (through the registry) two share one.
			m_EntityMap.emplace(m_Registry.get<IDComponent>(handle).Id, handle);
		}
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
		{
			return;
		}

		if (m_StageDepth > 0)
		{
			// A script may be running on this entity or its children: stop the whole subtree now, remove it later. Each
			// member gets its own command, so a child moved out of the subtree before the stage ends still goes.
			std::vector<Entity> subtree;
			GetSubtree(entity, subtree);
			if (subtree.empty())
			{
				subtree.push_back(entity); // not in the list (a raw registry entity)
			}
			for (Entity member : subtree)
			{
				if (m_PendingDestroy.insert((entt::entity)member).second)
				{
					m_Deferred.push_back({ DeferredCommand::Type::DestroyEntity, (entt::entity)member });
				}
			}
			return;
		}

		DestroyEntityNow(entity);

		// What the hooks DestroyEntityNow ran (trigger exits) asked for was queued: apply it now.
		if (m_Deferred.empty() == false)
		{
			ApplyDeferred();
		}
	}

	void Scene::DestroyEntityNow(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
		{
			m_PendingDestroy.erase((entt::entity)entity);
			return;
		}

		// Children first, taken up front because the hooks that run while an entity goes may change the list; each is
		// checked again before it goes.
		std::vector<Entity> subtree;
		GetSubtree(entity, subtree);
		if (subtree.empty())
		{
			subtree.push_back(entity); // not in the list (a raw registry entity)
		}
		for (size_t i = subtree.size(); i-- > 0;)
		{
			DestroySingleEntityNow(subtree[i]);
		}
		m_Graph.CheckAfterEdit();
	}

	void Scene::DestroySingleEntityNow(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
		{
			m_PendingDestroy.erase((entt::entity)entity);
			return;
		}

		m_PendingDestroy.erase((entt::entity)entity);
		// Its physics body goes with it (a body whose entity is gone would be written back to nothing).
		if (m_Physics != nullptr)
		{
			m_Physics->OnEntityDestroyed((entt::entity)entity);
		}
		if (m_Physics3D != nullptr)
		{
			// Triggers a destroyed character was inside get OnTriggerExit now, while it is still valid (#150). What their
			// scripts do is queued, so the entity can't be destroyed twice.
			const std::vector<entt::entity> triggersLeft = m_Physics3D->OnEntityDestroyed((entt::entity)entity);
			m_StageDepth++;
			for (entt::entity trigger : triggersLeft)
			{
				NotifyScripts(trigger, (entt::entity)entity, &ScriptableEntity::OnTriggerExit);
			}
			m_StageDepth--;
		}
		// Let native scripts clean up before their entity disappears.
		if (entity.HasComponent<NativeScriptComponent>())
		{
			for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
			{
				if (script.Instance != nullptr)
				{
					script.Instance->OnDestroy();
				}
			}
		}

		// Looked up until now, so a script's OnDestroy can still find its own entity by UUID.
		if (m_Registry.has<IDComponent>(entity))
		{
			const UUID uuid = m_Registry.get<IDComponent>(entity).Id;
			const auto found = m_EntityMap.find(uuid);
			if (found != m_EntityMap.end() && found->second == (entt::entity)entity)
			{
				m_EntityMap.erase(found);
			}
		}

		m_Graph.Remove((entt::entity)entity);
		m_Registry.destroy(entity);
	}

	bool Scene::IsPendingDestroy(Entity entity) const
	{
		return (bool)entity && m_PendingDestroy.find((entt::entity)entity) != m_PendingDestroy.end();
	}

	void Scene::RemoveScript(Entity entity, size_t index)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
		{
			return;
		}

		auto& scripts = entity.GetComponent<NativeScriptComponent>().Scripts;
		if (index >= scripts.size())
		{
			return;
		}

		if (m_StageDepth > 0)
		{
			scripts[index].PendingRemove = true;
			m_Deferred.push_back({ DeferredCommand::Type::RemoveFlaggedScripts, (entt::entity)entity });
			return;
		}

		if (scripts[index].Instance != nullptr)
		{
			scripts[index].Instance->OnDestroy();
		}

		scripts.erase(scripts.begin() + index);
	}

	void Scene::RemoveScripts(Entity entity)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
		{
			return;
		}

		if (m_StageDepth > 0)
		{
			for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
			{
				script.PendingRemove = true;
			}

			m_Deferred.push_back({ DeferredCommand::Type::RemoveFlaggedScripts, (entt::entity)entity });
			return;
		}

		for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
		{
			if (script.Instance != nullptr)
			{
				script.Instance->OnDestroy();
			}
		}

		entity.RemoveComponent<NativeScriptComponent>();
	}

	void Scene::RemoveFlaggedScriptsNow(entt::entity handle)
	{
		if (m_Registry.valid(handle) == false || m_Registry.has<NativeScriptComponent>(handle) == false)
		{
			return;
		}

		// OnDestroy can call back into the scene (queued, since a stage is open) and can bind scripts,
		// so no reference into the vector is kept across it: take one flagged script at a time out first.
		for (;;)
		{
			auto& scripts = m_Registry.get<NativeScriptComponent>(handle).Scripts;
			auto flagged = std::find_if(scripts.begin(), scripts.end(), [](const NativeScriptComponent::Script& script)
			{
				return script.PendingRemove;
			});
			if (flagged == scripts.end())
			{
				break;
			}

			std::unique_ptr<ScriptableEntity> instance = std::move(flagged->Instance);
			scripts.erase(flagged);
			if (instance != nullptr)
			{
				instance->OnDestroy();
			}
		}
	}

	void Scene::BeginStage()
	{
		m_StageDepth++;
	}

	void Scene::EndStage()
	{
		m_StageDepth--;
		if (m_StageDepth == 0)
		{
			ApplyDeferred();
		}
	}

	void Scene::ApplyDeferred()
	{
		// A stage stays open while this runs, so commands queued by OnDestroy callbacks wait in
		// m_Deferred and are picked up by the next round of the loop.
		m_StageDepth++;

		while (m_Deferred.empty() == false)
		{
			std::vector<DeferredCommand> batch = std::move(m_Deferred);
			m_Deferred.clear();

			for (const DeferredCommand& command : batch)
			{
				if (command.Kind == DeferredCommand::Type::RemoveFlaggedScripts)
				{
					RemoveFlaggedScriptsNow(command.Handle);
				}
				else
				{
					DestroyEntityNow(Entity{ command.Handle, this });
				}
			}
		}

		m_StageDepth--;
	}

	// One context per update, shared by every stage of that frame.
	static SystemContext MakeSystemContext(float deltaTime, bool isPlaying)
	{
		SystemContext context;
		context.DeltaTime = deltaTime;
		context.FrameIndex = Time::GetFrameCount();
		context.IsPlaying = isPlaying;
		return context;
	}

	void Scene::NotifyScripts(entt::entity self, entt::entity other, void (ScriptableEntity::*hook)(Entity))
	{
		if (m_Registry.valid(self) == false || m_Registry.valid(other) == false || m_Registry.has<NativeScriptComponent>(self) == false)
		{
			return;
		}
		if (IsPendingDestroy(Entity{ self, this }))
		{
			return;
		}

		// A snapshot: a script may add components, which can move the script list while it is walked.
		// Scripts removed meanwhile stay alive until the stage ends (RemoveScript is deferred).
		std::vector<ScriptableEntity*> instances;
		for (auto& script : m_Registry.get<NativeScriptComponent>(self).Scripts)
		{
			if (script.Instance != nullptr && script.PendingRemove == false)
			{
				instances.push_back(script.Instance.get());
			}
		}

		for (ScriptableEntity* instance : instances)
		{
			(instance->*hook)(Entity{ other, this });
		}
	}

	void Scene::DispatchUIEvents(std::vector<UIEvent>& events)
	{
		if (events.empty())
		{
			return;
		}

		BeginStage();
		std::vector<ScriptableEntity*> instances;
		for (const UIEvent& event : events)
		{
			bool handled = false;
			for (Entity current = event.Target; (bool)current && handled == false; current = GetParent(current))
			{
				if (m_Registry.valid(current) == false || IsPendingDestroy(current))
				{
					break;
				}
				if (m_Registry.has<NativeScriptComponent>(current) == false)
				{
					continue;
				}

				// A snapshot, as in NotifyScripts: a script may change the script list while it is walked.
				instances.clear();
				for (auto& script : m_Registry.get<NativeScriptComponent>(current).Scripts)
				{
					if (script.Instance != nullptr && script.PendingRemove == false)
					{
						instances.push_back(script.Instance.get());
					}
				}
				for (ScriptableEntity* instance : instances)
				{
					handled |= instance->OnUIEvent(event);
				}
			}
		}
		events.clear();
		EndStage();
	}

	void Scene::DispatchPhysicsEvents()
	{
		const bool events2D = m_Physics != nullptr && m_Physics->GetEvents().empty() == false;
		const bool events3D = m_Physics3D != nullptr && m_Physics3D->GetEvents().empty() == false;
		if (events2D == false && events3D == false)
		{
			return;
		}

		// Copied: nothing may change the lists while scripts run, but a copy costs little and can't dangle.
		const std::vector<PhysicsScene::Event> events = events2D ? m_Physics->GetEvents() : std::vector<PhysicsScene::Event>();
		const std::vector<PhysicsScene3D::Event> events3DCopy = events3D ? m_Physics3D->GetEvents() : std::vector<PhysicsScene3D::Event>();

		BeginStage();
		for (const PhysicsScene::Event& event : events)
		{
			switch (event.Kind)
			{
			case PhysicsScene::Event::Type::CollisionBegin:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnCollisionEnter);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnCollisionEnter);
				break;
			case PhysicsScene::Event::Type::CollisionEnd:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnCollisionExit);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnCollisionExit);
				break;
			case PhysicsScene::Event::Type::TriggerBegin:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnTriggerEnter);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnTriggerEnter);
				break;
			case PhysicsScene::Event::Type::TriggerEnd:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnTriggerExit);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnTriggerExit);
				break;
			}
		}
		// 3D: a character walking into or out of a trigger. Both get the hook, as in 2D.
		for (const PhysicsScene3D::Event& event : events3DCopy)
		{
			const bool begin = event.Kind == PhysicsScene3D::Event::Type::TriggerBegin;
			void (ScriptableEntity::*hook)(Entity) = begin ? &ScriptableEntity::OnTriggerEnter : &ScriptableEntity::OnTriggerExit;
			NotifyScripts(event.A, event.B, hook);
			NotifyScripts(event.B, event.A, hook);
		}
		EndStage();
	}

	void Scene::OnRuntimeStart()
	{
		// The physics worlds are built from world transforms: a parented collider starts where it is drawn. The UI is
		// laid out too, so the first frame's UI input works on real rects.
		ResolveTransforms();
		ResolveUILayout();
		if (m_Physics == nullptr)
		{
			m_Physics = CreateScope<PhysicsScene>(*this);
		}
		if (m_Physics3D == nullptr)
		{
			m_Physics3D = CreateScope<PhysicsScene3D>(*this);
		}
		if (m_Particles == nullptr)
		{
			m_Particles = CreateScope<ParticleScene>();
		}
		if (m_UI == nullptr)
		{
			m_UI = CreateScope<UIScene>();
		}
		// The play copy carries the editor scene's runtime state; the game starts from the saved settings.
		ResetSpriteAnimations(*this);
	}

	void Scene::OnRuntimeStop()
	{
		m_UI.reset();
		m_UIEvents.clear();
		m_Particles.reset();
		m_Physics3D.reset();
		m_Physics.reset();
	}

	const std::vector<SceneStage>& Scene::GetStages()
	{
		// Written here so the stages may use the scene's private parts.
		static const std::vector<SceneStage> s_Stages = {
			// 0. UI input: focus moves, Submit and Cancel reach the scripts (OnUIEvent) before their OnUpdate.
			{ "UI input", false, [](Scene& scene, const SystemContext& context)
			{
				scene.m_UI->Update(scene, context, scene.m_UIEvents);
				scene.DispatchUIEvents(scene.m_UIEvents);
			} },
			// 1. Native scripts. Instances are created lazily on their first update.
			{ "Scripts", false, &UpdateScripts },
			// 2. Physics: after the scripts have set velocities, moved kinematic bodies and told characters where to
			// go, before animation and transforms read the result. 2D (Box2D), then 3D (Jolt); their events together.
			{ "Physics", false, [](Scene& scene, const SystemContext& context)
			{
				scene.m_Physics->Step(context);
				scene.m_Physics3D->Step(context);
				scene.DispatchPhysicsEvents();
			} },
			// 3. Sprite animation, after the scripts so what a script started this frame shows this frame.
			{ "Sprite animation", false, &UpdateSpriteAnimations },
			// 4. Transforms: every WorldTransformComponent from the hierarchy, shared with the editor.
			{ "Transforms", true, [](Scene& scene, const SystemContext&)
			{
				scene.ResolveTransforms();
			} },
			// 5. Particles, after transforms so emitters read their final position this frame. Finished
			// one-shot effects (DestroyWhenFinished) are destroyed here, after the emitter loop.
			{ "Particles", false, &UpdateParticles },
			// 6. UI layout: every UI element's rect, after the scripts changed rects and visibility this frame.
			{ "UI layout", true, [](Scene& scene, const SystemContext&)
			{
				scene.ResolveUILayout();
			} },
		};
		return s_Stages;
	}

	void Scene::OnUpdateRuntime(float deltaTime)
	{
		const SystemContext context = MakeSystemContext(deltaTime, true);

		// Nobody called OnRuntimeStart: do it before the first stage, so it can't undo what the
		// scripts start in their first update (it resets the animators).
		if (m_Physics == nullptr)
		{
			OnRuntimeStart();
		}

		for (const SceneStage& stage : GetStages())
		{
			stage.Run(*this, context);
		}
	}

	void Scene::OnUpdateEditor(float deltaTime)
	{
		// Only the stages that keep the scene consistent while editing.
		const SystemContext context = MakeSystemContext(deltaTime, false);
		for (const SceneStage& stage : GetStages())
		{
			if (stage.RunsInEditor)
			{
				stage.Run(*this, context);
			}
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entityHandle : view)
		{
			if (view.get<CameraComponent>(entityHandle).Primary)
			{
				return Entity{ entityHandle, this };
			}
		}

		return {};
	}
	
	Entity Scene::GetMainLight()
	{
		Entity first;
		Entity main;
		uint32_t marked = 0;
		// Entity order (what the hierarchy shows), so "the first" is stable.
		for (entt::entity handle : m_Graph.GetList())
		{
			if (m_Registry.valid(handle) == false || m_Registry.has<DirectionalLightComponent>(handle) == false)
			{
				continue;
			}
			if ((bool)first == false)
			{
				first = Entity{ handle, this };
			}
			if (m_Registry.get<DirectionalLightComponent>(handle).IsMainLight)
			{
				if (marked == 0)
				{
					main = Entity{ handle, this };
				}
				marked++;
			}
		}

		if (marked > 1 && m_MainLightWarned == false)
		{
			HY_CORE_WARN("Scene '{}': {} directional lights are marked as the main light; the first is used", m_Name, marked);
			m_MainLightWarned = true;
		}
		return marked > 0 ? main : first;
	}

	void Scene::OnRender()
	{
	}
}
