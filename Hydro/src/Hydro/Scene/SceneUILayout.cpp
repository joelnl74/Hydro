#include <hypch.h>
#include "SceneUILayout.h"
#include "Entity.h"
#include "Scene.h"
#include "SceneGraph.h"
#include "SceneTransforms.h"
#include "UI/UIScene.h"
#include "Hydro/Renderer/Camera.h"

#include <algorithm>
#include <string>

namespace Hydro
{
	static void AddResolvedRect(entt::registry& registry, entt::entity entity)
	{
		registry.emplace_or_replace<UIResolvedRectComponent>(entity);
	}

	static void RemoveResolvedRect(entt::registry& registry, entt::entity entity)
	{
		registry.remove_if_exists<UIResolvedRectComponent>(entity);
	}

	SceneUILayout::SceneUILayout(Scene& scene, SceneGraph& graph, SceneTransforms& transforms)
		: m_Scene(scene), m_Graph(graph), m_Transforms(transforms)
	{
		m_Scene.m_Registry.on_construct<UIRectComponent>().connect<&AddResolvedRect>();
		m_Scene.m_Registry.on_destroy<UIRectComponent>().connect<&RemoveResolvedRect>();
	}

	void SceneUILayout::Resolve()
	{
		m_Graph.Ensure();

		const glm::vec2 viewSize{ (float)m_Scene.GetViewportWidth(), (float)m_Scene.GetViewportHeight() };
		const size_t count = m_Graph.GetList().size();
		m_CanvasMetrics.clear();
		m_WorldToViewState = 0;
		m_Entries.resize(count);
		// Rects a layout group placed for its children, written when the group is reached (before its children).
		m_Driven.assign(count, DrivenRect{});
		size_t written = 0;
		for (size_t i = 0; i < count; i++)
		{
			const entt::entity handle = m_Graph.GetList()[i];
			const int32_t parent = m_Graph.GetLayout().GetParentIndex(i);
			const Entry inherited = parent >= 0 ? m_Entries[(size_t)parent] : Entry{};
			Entry& entry = m_Entries[i];
			entry = inherited; // an entity without a rect passes its parent's on

			if (m_Scene.m_Registry.has<UICanvasComponent>(handle))
			{
				if (inherited.InCanvas)
				{
					// Nested canvases aren't supported: this one and everything on it is left out.
					if (m_NestedCanvasWarned.insert(handle).second)
					{
						const std::string name = m_Scene.m_Registry.has<TagComponent>(handle)
							? m_Scene.m_Registry.get<TagComponent>(handle).Tag
							: std::string("?");
						HY_CORE_WARN("UI: canvas '{}' is inside another canvas; nested canvases aren't supported, it isn't drawn", name);
					}
					entry.Canvas = 0;
					entry.Visible = false;
				}
				else
				{
					const UICanvasComponent& canvas = m_Scene.m_Registry.get<UICanvasComponent>(handle);
					UICanvasMetrics metrics;
					const bool placed = PlaceCanvas(i, canvas, viewSize, metrics);
					entry.Min = glm::vec2(0.0f);
					entry.Max = metrics.Size;
					entry.Visible = canvas.Visible && placed;
					entry.Canvas = m_Scene.m_Registry.has<IDComponent>(handle) ? m_Scene.m_Registry.get<IDComponent>(handle).Id : UUID(0);
					if (entry.Visible && (uint64_t)entry.Canvas != 0)
					{
						m_CanvasMetrics[(uint64_t)entry.Canvas] = metrics;
					}
				}
				entry.InCanvas = true;

				// A rect on the canvas itself is the whole canvas.
				if (m_Scene.m_Registry.has<UIRectComponent>(handle))
				{
					UIResolvedRectComponent& resolved = m_Scene.m_Registry.get<UIResolvedRectComponent>(handle);
					resolved.Min = entry.Min;
					resolved.Max = entry.Max;
					resolved.Visible = entry.Visible && m_Scene.m_Registry.get<UIRectComponent>(handle).Visible;
					resolved.Canvas = entry.Canvas;
					resolved.ClipMin = glm::vec2(-1.0e9f);
					resolved.ClipMax = glm::vec2(1.0e9f);
					written++;
				}
				continue;
			}

			if (m_Scene.m_Registry.has<UIRectComponent>(handle) == false)
			{
				continue;
			}

			HY_CORE_ASSERT(m_Scene.m_Registry.has<UIResolvedRectComponent>(handle),
				"Scene: an entity has a UIRectComponent but no UIResolvedRectComponent");
			const UIRectComponent& rect = m_Scene.m_Registry.get<UIRectComponent>(handle);
			UIResolvedRectComponent& resolved = m_Scene.m_Registry.get<UIResolvedRectComponent>(handle);
			if ((uint64_t)inherited.Canvas == 0)
			{
				// No canvas above it: laid out as nothing, never drawn.
				resolved = UIResolvedRectComponent{};
				entry.Visible = false;
			}
			else
			{
				if (m_Driven[i].Driven)
				{
					entry.Min = m_Driven[i].Min;
					entry.Max = m_Driven[i].Max;
				}
				else
				{
					UILayout::ResolveRect(rect, inherited.Min, inherited.Max, entry.Min, entry.Max);
				}
				entry.Visible = inherited.Visible && rect.Visible;
				if (m_Scene.m_Registry.has<UILayoutGroupComponent>(handle))
				{
					ArrangeChildren(i, rect, m_Scene.m_Registry.get<UILayoutGroupComponent>(handle), entry.Min, entry.Max);
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
		if (m_Scene.m_Registry.size<UIScrollViewComponent>() > 0)
		{
			for (size_t i = 0; i < count; i++)
			{
				const entt::entity handle = m_Graph.GetList()[i];
				if (m_Scene.m_Registry.has<UIScrollViewComponent>(handle) && m_Scene.m_Registry.has<UIResolvedRectComponent>(handle))
				{
					ScrollView(i);
				}
			}
		}

		// Elements outside the list (raw registry entities without an IDComponent) have no canvas.
		if (written < m_Scene.m_Registry.size<UIRectComponent>())
		{
			auto view = m_Scene.m_Registry.view<UIRectComponent, UIResolvedRectComponent>();
			for (entt::entity handle : view)
			{
				if (m_Graph.IsListed(handle) == false)
				{
					view.get<UIResolvedRectComponent>(handle) = UIResolvedRectComponent{};
				}
			}
		}
	}

	bool SceneUILayout::GetCanvasMetrics(UUID canvas, UICanvasMetrics& metrics) const
	{
		const auto found = m_CanvasMetrics.find((uint64_t)canvas);
		if (found == m_CanvasMetrics.end())
		{
			return false;
		}
		metrics = found->second;
		return true;
	}

	bool SceneUILayout::PlaceCanvas(size_t index, const UICanvasComponent& canvas, const glm::vec2& viewSize, UICanvasMetrics& metrics)
	{
		metrics = UILayout::GetCanvasMetrics(canvas, viewSize);
		if (canvas.Space != UICanvasSpace::World)
		{
			return true;
		}

		// World: a WorldSize box at the canvas scale, its pivot on where the primary camera sees the entity.
		metrics.Size = glm::max(canvas.WorldSize, glm::vec2(1.0f));
		if (m_WorldToViewState == 0)
		{
			m_WorldToViewState = -1;
			Entity camera = m_Scene.GetPrimaryCameraEntity();
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
				const glm::mat4 projection = camera.GetComponent<CameraComponent>().Camera.GetProjection(m_Scene.GetViewportAspectRatio());
				const glm::mat4 screen = Camera::Orthographic(0.0f, viewSize.x, 0.0f, viewSize.y, -1.0f, 1.0f);
				m_WorldToView = glm::inverse(screen) * projection * glm::inverse(cameraTransform);
				m_WorldToViewState = 1;
			}
		}
		if (m_WorldToViewState < 0)
		{
			return false;
		}

		// The entity's world position (a canvas without a transform of its own sits where its parent is).
		const std::vector<glm::mat4>& resolved = m_Transforms.GetResolvedMatrices();
		const glm::mat4 world = resolved.size() == m_Graph.GetList().size() ? resolved[index]
			: m_Scene.GetWorldTransform(Entity{ m_Graph.GetList()[index], &m_Scene });
		const glm::vec4 clip = m_WorldToView * glm::vec4(glm::vec3(world[3]), 1.0f);
		if (clip.w <= 1.0e-5f)
		{
			return false; // behind the camera
		}
		const glm::vec2 point = glm::vec2(clip) / clip.w;
		metrics.Offset = glm::round(point - canvas.WorldPivot * metrics.Size * metrics.Scale);
		return true;
	}

	void SceneUILayout::ScrollView(size_t index)
	{
		const entt::entity handle = m_Graph.GetList()[index];
		UIScrollViewComponent& scroll = m_Scene.m_Registry.get<UIScrollViewComponent>(handle);
		const UIResolvedRectComponent view = m_Scene.m_Registry.get<UIResolvedRectComponent>(handle);
		if ((uint64_t)view.Canvas == 0)
		{
			return;
		}

		// The content: the visible UI children's rects together, as laid out (unscrolled).
		m_Graph.GetLayout().GetChildIndices(index, m_ChildIndices);
		bool any = false;
		float contentBottom = 0.0f;
		float contentTop = 0.0f;
		for (size_t child : m_ChildIndices)
		{
			const entt::entity childHandle = m_Graph.GetList()[child];
			if (m_Scene.m_Registry.has<UIResolvedRectComponent>(childHandle) == false
				|| m_Scene.m_Registry.has<UICanvasComponent>(childHandle))
			{
				continue;
			}
			const UIResolvedRectComponent& rect = m_Scene.m_Registry.get<UIResolvedRectComponent>(childHandle);
			if (m_Scene.m_Registry.get<UIRectComponent>(childHandle).Visible == false)
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
		if (scroll.FollowFocus && m_Scene.GetUIScene() != nullptr)
		{
			const Entity focused = m_Scene.GetUIScene()->GetFocused(m_Scene);
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
			if (m_Scene.m_Registry.has<UIResolvedRectComponent>(element) == false)
			{
				continue;
			}
			UIResolvedRectComponent& rect = m_Scene.m_Registry.get<UIResolvedRectComponent>(element);
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
		Entity thumb = (uint64_t)scroll.Scrollbar != 0 ? m_Scene.GetEntityByUUID(scroll.Scrollbar) : Entity{};
		if ((bool)thumb == false || thumb.HasComponent<UIResolvedRectComponent>() == false)
		{
			return;
		}
		UIResolvedRectComponent& thumbRect = thumb.GetComponent<UIResolvedRectComponent>();
		const Entity track = m_Scene.GetParent(thumb);
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

	void SceneUILayout::ArrangeChildren(size_t index, const UIRectComponent& rect, const UILayoutGroupComponent& group, glm::vec2& min,
		glm::vec2& max)
	{
		// The visible UI children (not nested canvases), at the sizes their own rects give them in the group's rect.
		m_Graph.GetLayout().GetChildIndices(index, m_ChildIndices);
		m_Items.clear();
		m_ItemIndices.clear();
		for (size_t child : m_ChildIndices)
		{
			const entt::entity handle = m_Graph.GetList()[child];
			if (m_Scene.m_Registry.has<UIRectComponent>(handle) == false || m_Scene.m_Registry.has<UICanvasComponent>(handle))
			{
				continue;
			}
			const UIRectComponent& childRect = m_Scene.m_Registry.get<UIRectComponent>(handle);
			if (childRect.Visible == false)
			{
				continue; // hidden: no room, the gap closes
			}
			UILayoutItem item;
			glm::vec2 childMin;
			glm::vec2 childMax;
			UILayout::ResolveRect(childRect, min, max, childMin, childMax);
			item.Size = childMax - childMin;
			m_Items.push_back(item);
			m_ItemIndices.push_back(child);
		}

		UILayout::ArrangeGroup(group, rect.Pivot, min, max, m_Items);
		for (size_t k = 0; k < m_Items.size(); k++)
		{
			DrivenRect& driven = m_Driven[m_ItemIndices[k]];
			driven.Driven = true;
			driven.Min = m_Items[k].Min;
			driven.Max = m_Items[k].Max;
		}
	}
}
