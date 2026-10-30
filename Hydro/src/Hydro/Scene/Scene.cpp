#include <hypch.h>
#include "Scene.h"
#include "Entity.h"
#include "Scripting/ScriptableEntity.h"
#include "Scripting/ScriptSystem.h"
#include "SpriteAnimator.h"
#include "SystemContext.h"
#include "TransformMath.h"
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
	// WorldTransformComponent comes and goes with TransformComponent (registry signals, connected in the
	// constructor), so ResolveTransforms never adds or removes components.
	static void AddWorldTransform(entt::registry& registry, entt::entity entity)
	{
		registry.emplace_or_replace<WorldTransformComponent>(entity);
	}

	static void RemoveWorldTransform(entt::registry& registry, entt::entity entity)
	{
		registry.remove_if_exists<WorldTransformComponent>(entity);
	}

	// UIResolvedRectComponent comes and goes with UIRectComponent the same way, so ResolveUILayout only writes values.
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
		m_Registry.on_construct<TransformComponent>().connect<&AddWorldTransform>();
		m_Registry.on_destroy<TransformComponent>().connect<&RemoveWorldTransform>();
		m_Registry.on_construct<UIRectComponent>().connect<&AddResolvedRect>();
		m_Registry.on_destroy<UIRectComponent>().connect<&RemoveResolvedRect>();
	}

	Scene::~Scene()
	{
		// Give scripts a chance to clean up while the registry and their entities still exist. A stage
		// is open so that anything a script's OnDestroy asks for (DestroyEntity...) is queued and then
		// dropped instead of changing the registry under this loop.
		m_StageDepth++;

		auto view = m_Registry.view<NativeScriptComponent>();
		for (auto entityHandle : view)
		{
			auto& scriptComponent = view.get<NativeScriptComponent>(entityHandle);
			for (auto& script : scriptComponent.Scripts)
			{
				if (script.Instance != nullptr)
					script.Instance->OnDestroy();
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
					destination.emplace_or_replace<Component>(it->second, view.template get<const Component>(sourceEntity));
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

		// Native scripts: copy the bindings, not the instances (those belong to the source scene).
		auto scripts = sourceRegistry.view<const NativeScriptComponent>();
		for (auto sourceEntity : scripts)
		{
			auto it = entityMap.find(sourceEntity);
			if (it == entityMap.end())
				continue;

			NativeScriptComponent& copied = copy->m_Registry.emplace<NativeScriptComponent>(it->second);
			for (const auto& script : scripts.get<const NativeScriptComponent>(sourceEntity).Scripts)
			{
				NativeScriptComponent::Script binding;
				binding.Name = script.Name;
				binding.Instantiate = script.Instantiate;
				binding.Properties = script.Properties;
				copied.Scripts.push_back(std::move(binding));
			}
		}

		return copy;
	}

	template<typename... Component>
	static void CopyComponentsOfEntity(ComponentGroup<Component...>, entt::registry& registry, entt::entity source, entt::entity destination)
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
			bindings.push_back(std::move(binding));
		}
		registry.emplace_or_replace<NativeScriptComponent>(destination).Scripts = std::move(bindings);
	}

	void Scene::RemapEntityReferences(const std::vector<Entity>& entities, const std::unordered_map<UUID, UUID>& remap)
	{
		auto remapped = [&remap](UUID id)
		{
			const auto found = remap.find(id);
			return found != remap.end() ? found->second : id;
		};

		// One line per field that refers to an entity; the component registry (Roadmap A6) will let each component
		// register its own instead.
		for (Entity entity : entities)
		{
			if (m_Registry.valid(entity) == false)
			{
				continue;
			}
			if (m_Registry.has<RelationshipComponent>(entity))
			{
				RelationshipComponent& relationship = m_Registry.get<RelationshipComponent>(entity);
				const UUID parent = remapped(relationship.Parent);
				if (parent != relationship.Parent)
				{
					relationship.Parent = parent;
					m_HierarchyDirty = true;
				}
			}
			if (m_Registry.has<UIButtonComponent>(entity))
			{
				UIButtonComponent& button = m_Registry.get<UIButtonComponent>(entity);
				button.Up = remapped(button.Up);
				button.Down = remapped(button.Down);
				button.Left = remapped(button.Left);
				button.Right = remapped(button.Right);
			}
			if (m_Registry.has<UIScrollViewComponent>(entity))
			{
				UIScrollViewComponent& scroll = m_Registry.get<UIScrollViewComponent>(entity);
				scroll.Scrollbar = remapped(scroll.Scrollbar);
			}
		}
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
			CopyScriptBindings(m_Registry, original, copy);
			remap[original.GetUUID()] = copy.GetUUID();
			copies.push_back(copy);
		}
		RemapEntityReferences(copies, remap);
		m_HierarchyDirty = true;

		// Right after the original's subtree. The copy of `entity` kept its parent, so it is a sibling there.
		const int32_t originalIndex = GetOrderIndex(entity);
		const int32_t copyIndex = GetOrderIndex((entt::entity)copies.front());
		MoveSubtree((size_t)copyIndex, m_Hierarchy.GetSiblingInsertIndex((size_t)originalIndex, true));
		CheckHierarchyAfterEdit();

		return copies.front();
	}

	const std::vector<entt::entity>& Scene::GetEntityOrder()
	{
		// Drop entities that were destroyed straight through the registry.
		const size_t listed = m_EntityOrder.size();
		m_EntityOrder.erase(std::remove_if(m_EntityOrder.begin(), m_EntityOrder.end(), [&](entt::entity handle)
		{
			return m_Registry.valid(handle) == false || m_Registry.has<IDComponent>(handle) == false;
		}), m_EntityOrder.end());
		if (m_EntityOrder.size() != listed)
		{
			m_HierarchyDirty = true;
		}

		// Pick up entities that got an IDComponent without going through CreateEntity.
		if (m_Registry.size<IDComponent>() != m_EntityOrder.size())
		{
			const std::unordered_set<entt::entity> known(m_EntityOrder.begin(), m_EntityOrder.end());
			std::vector<entt::entity> missing;
			for (entt::entity handle : m_Registry.view<IDComponent>())
			{
				if (known.find(handle) == known.end())
				{
					missing.push_back(handle);
				}
			}

			// The view lists newest first; the list is oldest first.
			m_EntityOrder.insert(m_EntityOrder.end(), missing.rbegin(), missing.rend());
			m_HierarchyDirty = true;
		}

		return m_EntityOrder;
	}

	void Scene::MoveEntity(Entity entity, Entity target, bool placeAfter)
	{
		if ((bool)entity == false || (bool)target == false || entity == target)
		{
			return;
		}

		const int32_t entityIndex = GetOrderIndex((entt::entity)entity);
		const int32_t targetIndex = GetOrderIndex((entt::entity)target);
		if (entityIndex < 0 || targetIndex < 0)
		{
			return;
		}
		if (m_Hierarchy.IsAncestor((size_t)entityIndex, (size_t)targetIndex))
		{
			HY_CORE_WARN("Scene '{}': can't move '{}' next to '{}', which is inside it", m_Name, entity.GetName(), target.GetName());
			return;
		}

		// It becomes target's sibling, staying where it is in the world.
		UUID parent = 0;
		if (m_Registry.has<RelationshipComponent>(target))
		{
			parent = m_Registry.get<RelationshipComponent>(target).Parent;
		}
		UUID currentParent = 0;
		if (m_Registry.has<RelationshipComponent>(entity))
		{
			currentParent = m_Registry.get<RelationshipComponent>(entity).Parent;
		}
		const bool keepWorld = parent != currentParent && m_Registry.has<TransformComponent>(entity);
		const glm::mat4 world = keepWorld ? GetWorldTransform(entity) : glm::mat4(1.0f);

		const size_t insertAt = m_Hierarchy.GetSiblingInsertIndex((size_t)targetIndex, placeAfter);
		SetParentUUID(entity, parent);
		MoveSubtree((size_t)entityIndex, insertAt);
		if (keepWorld)
		{
			SetWorldTransform(entity, world);
		}
		CheckHierarchyAfterEdit();
	}

	// ---- Hierarchy ------------------------------------------------------------------------------

	std::vector<SceneHierarchy::Node> Scene::MakeHierarchyNodes()
	{
		std::vector<SceneHierarchy::Node> nodes;
		nodes.reserve(m_EntityOrder.size());
		for (entt::entity handle : m_EntityOrder)
		{
			SceneHierarchy::Node node;
			node.Id = m_Registry.get<IDComponent>(handle).Id;
			if (m_Registry.has<RelationshipComponent>(handle))
			{
				node.Parent = m_Registry.get<RelationshipComponent>(handle).Parent;
			}
			nodes.push_back(node);
		}
		return nodes;
	}

	void Scene::EnsureHierarchy()
	{
		// Drops destroyed entities and picks up unlisted ones (marks the hierarchy dirty if it did).
		GetEntityOrder();
		if (m_HierarchyDirty == false)
		{
			return;
		}

		m_Hierarchy.Build(MakeHierarchyNodes());
		m_OrderIndex.clear();
		m_OrderIndex.reserve(m_EntityOrder.size());
		for (size_t i = 0; i < m_EntityOrder.size(); i++)
		{
			m_OrderIndex[m_EntityOrder[i]] = (uint32_t)i;
		}
		m_HierarchyDirty = false;
	}

	int32_t Scene::GetOrderIndex(entt::entity handle)
	{
		EnsureHierarchy();
		const auto found = m_OrderIndex.find(handle);
		return found != m_OrderIndex.end() ? (int32_t)found->second : -1;
	}

	void Scene::SetParentUUID(entt::entity handle, UUID parent)
	{
		if ((uint64_t)parent == 0)
		{
			if (m_Registry.has<RelationshipComponent>(handle))
			{
				m_Registry.remove<RelationshipComponent>(handle);
			}
		}
		else
		{
			m_Registry.emplace_or_replace<RelationshipComponent>(handle, parent);
		}
		m_HierarchyDirty = true;
	}

	void Scene::MoveSubtree(size_t index, size_t insertAt)
	{
		SceneHierarchy::MoveRange(m_EntityOrder, index, m_Hierarchy.GetSubtreeEnd(index), insertAt);
		m_HierarchyDirty = true;
	}

	void Scene::CheckHierarchyAfterEdit()
	{
#ifdef HY_DEBUG
		ValidateHierarchy();
#endif
	}

	bool Scene::SetParent(Entity child, Entity parent, bool keepWorldTransform)
	{
		if ((bool)child == false || m_Registry.valid(child) == false)
		{
			return false;
		}

		const int32_t childIndex = GetOrderIndex((entt::entity)child);
		if (childIndex < 0)
		{
			HY_CORE_ERROR("Scene '{}': SetParent on an entity that isn't in the scene's list", m_Name);
			return false;
		}

		int32_t parentIndex = -1;
		UUID parentUUID = 0;
		if ((bool)parent)
		{
			parentIndex = m_Registry.valid(parent) ? GetOrderIndex((entt::entity)parent) : -1;
			if (parentIndex < 0)
			{
				HY_CORE_ERROR("Scene '{}': '{}' can't get a parent that isn't in the scene's list", m_Name, child.GetName());
				return false;
			}
			if (parentIndex == childIndex || m_Hierarchy.IsAncestor((size_t)childIndex, (size_t)parentIndex))
			{
				HY_CORE_ERROR("Scene '{}': '{}' can't be the parent of '{}', which contains it", m_Name, parent.GetName(), child.GetName());
				return false;
			}
			parentUUID = parent.GetUUID();
		}

		UUID currentParent = 0;
		if (m_Registry.has<RelationshipComponent>(child))
		{
			currentParent = m_Registry.get<RelationshipComponent>(child).Parent;
		}
		if (currentParent == parentUUID)
		{
			return true;
		}

		const bool keepWorld = keepWorldTransform && m_Registry.has<TransformComponent>(child);
		const glm::mat4 world = keepWorld ? GetWorldTransform(child) : glm::mat4(1.0f);

		const size_t insertAt = m_Hierarchy.GetLastChildInsertIndex(parentIndex);
		SetParentUUID(child, parentUUID);
		MoveSubtree((size_t)childIndex, insertAt);
		if (keepWorld)
		{
			SetWorldTransform(child, world);
		}
		CheckHierarchyAfterEdit();
		return true;
	}

	Entity Scene::GetParent(Entity entity)
	{
		if (entity.HasComponent<RelationshipComponent>() == false)
		{
			return {};
		}
		return GetEntityByUUID(entity.GetComponent<RelationshipComponent>().Parent);
	}

	void Scene::GetChildren(Entity entity, std::vector<Entity>& children)
	{
		children.clear();
		const int32_t index = (bool)entity ? GetOrderIndex((entt::entity)entity) : -1;
		if (index < 0)
		{
			return;
		}

		std::vector<size_t> indices;
		m_Hierarchy.GetChildIndices((size_t)index, indices);
		for (size_t child : indices)
		{
			children.push_back(Entity{ m_EntityOrder[child], this });
		}
	}

	bool Scene::HasChildren(Entity entity)
	{
		const int32_t index = (bool)entity ? GetOrderIndex((entt::entity)entity) : -1;
		return index >= 0 && m_Hierarchy.HasChildren((size_t)index);
	}

	void Scene::GetSubtree(Entity entity, std::vector<Entity>& subtree)
	{
		subtree.clear();
		const int32_t index = (bool)entity ? GetOrderIndex((entt::entity)entity) : -1;
		if (index < 0)
		{
			return;
		}

		for (size_t i = (size_t)index; i < m_Hierarchy.GetSubtreeEnd((size_t)index); i++)
		{
			subtree.push_back(Entity{ m_EntityOrder[i], this });
		}
	}

	bool Scene::IsAncestorOf(Entity ancestor, Entity entity)
	{
		if ((bool)ancestor == false || (bool)entity == false)
		{
			return false;
		}

		const int32_t ancestorIndex = GetOrderIndex((entt::entity)ancestor);
		const int32_t entityIndex = GetOrderIndex((entt::entity)entity);
		return ancestorIndex >= 0 && entityIndex >= 0 && m_Hierarchy.IsAncestor((size_t)ancestorIndex, (size_t)entityIndex);
	}

	uint32_t Scene::GetDepth(Entity entity)
	{
		const int32_t index = (bool)entity ? GetOrderIndex((entt::entity)entity) : -1;
		return index >= 0 ? m_Hierarchy.GetDepth((size_t)index) : 0;
	}

	void Scene::RepairHierarchy()
	{
		GetEntityOrder();
		std::vector<SceneHierarchy::Node> nodes = MakeHierarchyNodes();
		const std::vector<SceneHierarchy::Node> before = nodes;

		std::vector<SceneHierarchy::Issue> fixed;
		const std::vector<size_t> order = SceneHierarchy::Repair(nodes, fixed);

		for (const SceneHierarchy::Issue& issue : fixed)
		{
			const entt::entity handle = m_EntityOrder[issue.Index];
			HY_CORE_WARN("Scene '{}': '{}' is a root now: {} (parent {})", m_Name, m_Registry.get<TagComponent>(handle).Tag,
				SceneHierarchy::ProblemToString(issue.Kind), (uint64_t)before[issue.Index].Parent);
			SetParentUUID(handle, nodes[issue.Index].Parent);
		}

		bool reordered = false;
		std::vector<entt::entity> repaired;
		repaired.reserve(order.size());
		for (size_t i = 0; i < order.size(); i++)
		{
			repaired.push_back(m_EntityOrder[order[i]]);
			if (order[i] != i)
			{
				reordered = true;
			}
		}
		if (reordered)
		{
			HY_CORE_WARN("Scene '{}': entities were reordered so every parent comes before its children", m_Name);
			m_EntityOrder = std::move(repaired);
			m_HierarchyDirty = true;
		}
		CheckHierarchyAfterEdit();
	}

	bool Scene::ValidateHierarchy()
	{
		GetEntityOrder();
		std::vector<SceneHierarchy::Issue> issues;
		if (SceneHierarchy::Validate(MakeHierarchyNodes(), &issues))
		{
			return true;
		}

		for (const SceneHierarchy::Issue& issue : issues)
		{
			HY_CORE_ERROR("Scene '{}': hierarchy broken at '{}': {}", m_Name, m_Registry.get<TagComponent>(m_EntityOrder[issue.Index]).Tag,
				SceneHierarchy::ProblemToString(issue.Kind));
		}
		return false;
	}

	// ---- World transforms -----------------------------------------------------------------------

	static void WriteWorldTransform(WorldTransformComponent& world, const glm::mat4& matrix)
	{
		world.Matrix = matrix;
		if (TransformMath::Decompose(matrix, world.Translation, world.Rotation, world.Scale) == false)
		{
			// A zero scale somewhere up the chain: there is no rotation to find; keep the position.
			world.Translation = glm::vec3(matrix[3]);
			world.Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			world.Scale = glm::vec3(0.0f);
		}
	}

	void Scene::ResolveTransforms()
	{
		EnsureHierarchy();

		static const glm::mat4 s_Identity(1.0f);
		const size_t count = m_EntityOrder.size();
		m_WorldMatrices.resize(count);
		size_t written = 0;
		for (size_t i = 0; i < count; i++)
		{
			const entt::entity handle = m_EntityOrder[i];
			const int32_t parent = m_Hierarchy.GetParentIndex(i);
			const glm::mat4& parentWorld = parent >= 0 ? m_WorldMatrices[(size_t)parent] : s_Identity;
			if (m_Registry.has<TransformComponent>(handle) == false)
			{
				// No transform of its own: its children are placed by its parent's.
				m_WorldMatrices[i] = parentWorld;
				continue;
			}

			const glm::mat4 local = m_Registry.get<TransformComponent>(handle).GetTransform();
			m_WorldMatrices[i] = parent >= 0 ? parentWorld * local : local;
			HY_CORE_ASSERT(m_Registry.has<WorldTransformComponent>(handle), "Scene: an entity has a TransformComponent but no WorldTransformComponent");
			WriteWorldTransform(m_Registry.get<WorldTransformComponent>(handle), m_WorldMatrices[i]);
			written++;
		}

		// Entities outside the list (raw registry entities without an IDComponent) have no parent.
		if (written < m_Registry.size<TransformComponent>())
		{
			auto view = m_Registry.view<TransformComponent, WorldTransformComponent>();
			for (entt::entity handle : view)
			{
				if (m_OrderIndex.find(handle) == m_OrderIndex.end())
				{
					WriteWorldTransform(view.get<WorldTransformComponent>(handle), view.get<TransformComponent>(handle).GetTransform());
				}
			}
		}
	}

	void Scene::ResolveUILayout()
	{
		EnsureHierarchy();

		const glm::vec2 viewSize{ (float)m_ViewportWidth, (float)m_ViewportHeight };
		const size_t count = m_EntityOrder.size();
		m_UICanvasMetrics.clear();
		m_UIWorldToViewState = 0;
		m_UILayout.resize(count);
		// Rects a layout group placed for its children, written when the group is reached (before its children).
		m_UIDriven.assign(count, UIDrivenRect{});
		size_t written = 0;
		for (size_t i = 0; i < count; i++)
		{
			const entt::entity handle = m_EntityOrder[i];
			const int32_t parent = m_Hierarchy.GetParentIndex(i);
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
						const std::string name = m_Registry.has<TagComponent>(handle) ? m_Registry.get<TagComponent>(handle).Tag : std::string("?");
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

			HY_CORE_ASSERT(m_Registry.has<UIResolvedRectComponent>(handle), "Scene: an entity has a UIRectComponent but no UIResolvedRectComponent");
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
				const entt::entity handle = m_EntityOrder[i];
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
				if (m_OrderIndex.find(handle) == m_OrderIndex.end())
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
		const glm::mat4 world = m_WorldMatrices.size() == m_EntityOrder.size() ? m_WorldMatrices[index]
			: GetWorldTransform(Entity{ m_EntityOrder[index], this });
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
		const entt::entity handle = m_EntityOrder[index];
		UIScrollViewComponent& scroll = m_Registry.get<UIScrollViewComponent>(handle);
		const UIResolvedRectComponent view = m_Registry.get<UIResolvedRectComponent>(handle);
		if ((uint64_t)view.Canvas == 0)
		{
			return;
		}

		// The content: the visible UI children's rects together, as laid out (unscrolled).
		m_Hierarchy.GetChildIndices(index, m_UIChildIndices);
		bool any = false;
		float contentBottom = 0.0f;
		float contentTop = 0.0f;
		for (size_t child : m_UIChildIndices)
		{
			const entt::entity childHandle = m_EntityOrder[child];
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
				const int32_t focusedIndex = GetOrderIndex((entt::entity)focused);
				const UIResolvedRectComponent& rect = focused.GetComponent<UIResolvedRectComponent>();
				if (focusedIndex >= 0 && m_Hierarchy.IsAncestor(index, (size_t)focusedIndex) && rect.Visible)
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
		const size_t end = m_Hierarchy.GetSubtreeEnd(index);
		for (size_t i = index + 1; i < end; i++)
		{
			const entt::entity element = m_EntityOrder[i];
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
		const float thumbHeight = std::round(std::clamp(trackHeight * viewHeight / std::max(viewHeight + maxScroll, 1.0f), std::min(8.0f, trackHeight), trackHeight));
		const float top = trackRect.Max.y - std::round((trackHeight - thumbHeight) * (position / maxScroll));
		thumbRect.Max.y = top;
		thumbRect.Min.y = top - thumbHeight;
	}

	void Scene::ArrangeUIChildren(size_t index, const UIRectComponent& rect, const UILayoutGroupComponent& group, glm::vec2& min, glm::vec2& max)
	{
		// The visible UI children (not nested canvases), at the sizes their own rects give them in the group's rect.
		m_Hierarchy.GetChildIndices(index, m_UIChildIndices);
		m_UIItems.clear();
		m_UIItemIndices.clear();
		for (size_t child : m_UIChildIndices)
		{
			const entt::entity handle = m_EntityOrder[child];
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

	glm::mat4 Scene::GetWorldTransform(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
		{
			return glm::mat4(1.0f);
		}

		const int32_t index = GetOrderIndex((entt::entity)entity);
		if (index < 0)
		{
			return m_Registry.has<TransformComponent>(entity) ? m_Registry.get<TransformComponent>(entity).GetTransform() : glm::mat4(1.0f);
		}

		// Up the chain: each ancestor's local transform goes on the left.
		glm::mat4 world(1.0f);
		for (int32_t i = index; i >= 0; i = m_Hierarchy.GetParentIndex((size_t)i))
		{
			const entt::entity handle = m_EntityOrder[(size_t)i];
			if (m_Registry.has<TransformComponent>(handle))
			{
				world = m_Registry.get<TransformComponent>(handle).GetTransform() * world;
			}
		}
		return world;
	}

	bool Scene::SetWorldTransform(Entity entity, const glm::mat4& world)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false || m_Registry.has<TransformComponent>(entity) == false)
		{
			return false;
		}

		glm::mat4 parentWorld(1.0f);
		const int32_t index = GetOrderIndex((entt::entity)entity);
		if (index >= 0)
		{
			const int32_t parent = m_Hierarchy.GetParentIndex((size_t)index);
			if (parent >= 0)
			{
				parentWorld = GetWorldTransform(Entity{ m_EntityOrder[(size_t)parent], this });
			}
		}
		if (std::abs(glm::determinant(glm::mat3(parentWorld))) < 1e-12f)
		{
			HY_CORE_WARN("Scene '{}': can't place '{}' in the world: a parent has a zero scale", m_Name, entity.GetName());
			return false;
		}

		const glm::mat4 local = glm::inverse(parentWorld) * world;
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
		if (TransformMath::Decompose(local, translation, rotation, scale) == false)
		{
			HY_CORE_WARN("Scene '{}': can't place '{}' in the world: the transform has a zero scale", m_Name, entity.GetName());
			return false;
		}

		TransformComponent& transform = m_Registry.get<TransformComponent>(entity);
		transform.Translation = translation;
		transform.Rotation = TransformMath::ToEulerAngles(rotation, transform.Rotation);
		transform.Scale = scale;

		if (TransformMath::IsTRS(local))
		{
			return true;
		}
		if (m_ShearWarned.insert((entt::entity)entity).second)
		{
			HY_CORE_WARN("Scene '{}': '{}' would need shear to stay in place (a rotated parent with non-uniform scale); it got the closest transform without",
				m_Name, entity.GetName());
		}
		return false;
	}

	glm::vec3 Scene::GetWorldPosition(Entity entity)
	{
		return glm::vec3(GetWorldTransform(entity)[3]);
	}

	bool Scene::SetWorldPosition(Entity entity, const glm::vec3& position)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false || m_Registry.has<TransformComponent>(entity) == false)
		{
			return false;
		}

		const int32_t index = GetOrderIndex((entt::entity)entity);
		const int32_t parent = index >= 0 ? m_Hierarchy.GetParentIndex((size_t)index) : -1;
		if (parent < 0)
		{
			m_Registry.get<TransformComponent>(entity).Translation = position;
			return true;
		}

		// world = parentWorld * translate(local) * rotation * scale: the world position is parentWorld applied to the
		// local Translation, so only that changes.
		const glm::mat4 parentWorld = GetWorldTransform(Entity{ m_EntityOrder[(size_t)parent], this });
		if (std::abs(glm::determinant(glm::mat3(parentWorld))) < 1e-12f)
		{
			return false;
		}
		m_Registry.get<TransformComponent>(entity).Translation = glm::vec3(glm::inverse(parentWorld) * glm::vec4(position, 1.0f));
		return true;
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

		m_EntityOrder.push_back((entt::entity)entity);
		m_EntityMap[uuid] = (entt::entity)entity;
		// A root at the end of the list: the rule still holds.
		m_HierarchyDirty = true;

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

			// A miss: unless the map matches the registry exactly, an entity may have gotten an
			// IDComponent without CreateEntity, so look at the registry once before saying no.
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
			// A script may be running on this very entity or one of its children: stop the whole subtree
			// from running again, remove it later. Each member gets its own command (the root first, which
			// takes the rest with it), so a child moved out of the subtree before the stage ends still goes.
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

		// The whole subtree, children first (the reverse of list order). Taken up front: the hooks that run
		// while an entity goes (OnDestroy, trigger exits) may change the list. Each is checked again before
		// it goes, since a hook may already have destroyed it.
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
		CheckHierarchyAfterEdit();
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
			// A character destroyed inside triggers leaves them (#150): their scripts get OnTriggerExit now, while the
			// character is still valid. Like a deferred stage, what they do to the scene is queued (applied by the
			// ApplyDeferred round this runs in, or by DestroyEntity), so the entity can't be destroyed twice.
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
					script.Instance->OnDestroy();
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

		m_EntityOrder.erase(std::remove(m_EntityOrder.begin(), m_EntityOrder.end(), (entt::entity)entity), m_EntityOrder.end());
		m_HierarchyDirty = true;
		m_Registry.destroy(entity);
	}

	bool Scene::IsPendingDestroy(Entity entity) const
	{
		return (bool)entity && m_PendingDestroy.find((entt::entity)entity) != m_PendingDestroy.end();
	}

	void Scene::RemoveScript(Entity entity, size_t index)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
			return;

		auto& scripts = entity.GetComponent<NativeScriptComponent>().Scripts;
		if (index >= scripts.size())
			return;

		if (m_StageDepth > 0)
		{
			scripts[index].PendingRemove = true;
			m_Deferred.push_back({ DeferredCommand::Type::RemoveFlaggedScripts, (entt::entity)entity });
			return;
		}

		if (scripts[index].Instance != nullptr)
			scripts[index].Instance->OnDestroy();

		scripts.erase(scripts.begin() + index);
	}

	void Scene::RemoveScripts(Entity entity)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
			return;

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
				script.Instance->OnDestroy();
		}

		entity.RemoveComponent<NativeScriptComponent>();
	}

	void Scene::RemoveFlaggedScriptsNow(entt::entity handle)
	{
		if (m_Registry.valid(handle) == false || m_Registry.has<NativeScriptComponent>(handle) == false)
			return;

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
				break;

			std::unique_ptr<ScriptableEntity> instance = std::move(flagged->Instance);
			scripts.erase(flagged);
			if (instance != nullptr)
				instance->OnDestroy();
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
					RemoveFlaggedScriptsNow(command.Handle);
				else
					DestroyEntityNow(Entity{ command.Handle, this });
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

	void Scene::OnUpdateRuntime(float deltaTime)
	{
		const SystemContext context = MakeSystemContext(deltaTime, true);

		// Nobody called OnRuntimeStart: do it before the first stage, so it can't undo what the
		// scripts start in their first update (it resets the animators).
		if (m_Physics == nullptr)
		{
			OnRuntimeStart();
		}

		// 0. UI input: focus moves, Submit and Cancel reach the scripts (OnUIEvent) before their OnUpdate.
		m_UI->Update(*this, context, m_UIEvents);
		DispatchUIEvents(m_UIEvents);

		// 1. Native scripts. Instances are created lazily on their first update.
		UpdateScripts(*this, context);

		// 2. Physics: after the scripts have set velocities, moved kinematic bodies and told characters where to
		// go, before animation and transforms read the result. 2D (Box2D), then 3D (Jolt); their events together.
		m_Physics->Step(context);
		m_Physics3D->Step(context);
		DispatchPhysicsEvents();

		// 3. Sprite animation, after the scripts so what a script started this frame shows this frame.
		UpdateSpriteAnimations(*this, context);

		// 4. Transforms: every WorldTransformComponent from the hierarchy, shared with the editor.
		ResolveTransforms();

		// 5. Particles, after transforms so emitters read their final position this frame. Finished
		// one-shot effects (DestroyWhenFinished) are destroyed here, after the emitter loop.
		UpdateParticles(*this, context);

		// 6. UI layout: every UI element's rect, after the scripts changed rects and visibility this frame.
		ResolveUILayout();
	}

	void Scene::OnUpdateEditor([[maybe_unused]] float deltaTime)
	{
		// Only the stages that keep the scene consistent while editing: no scripts, physics or animation.
		// 4. Transforms (world transforms from the hierarchy).
		ResolveTransforms();
		// 6. UI layout.
		ResolveUILayout();
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entityHandle : view)
		{
			if (view.get<CameraComponent>(entityHandle).Primary)
				return Entity{ entityHandle, this };
		}

		return {};
	}
	
	Entity Scene::GetMainLight()
	{
		Entity first;
		Entity main;
		uint32_t marked = 0;
		// Entity order (what the hierarchy shows), so "the first" is stable.
		for (entt::entity handle : m_EntityOrder)
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
