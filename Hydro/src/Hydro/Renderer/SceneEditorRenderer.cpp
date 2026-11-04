#include "hypch.h"

#include "SceneEditorRenderer.h"
#include "Renderer2D.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Scene/ColliderShapes.h"
#include "Hydro/Scene/MeshBounds.h"
#include "Hydro/Scene/Scripting/Gizmos.h"
#include "Hydro/Scene/Scripting/ScriptableEntity.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	void SceneEditorRenderer::Submit(Scene& scene, const SceneRenderer::EditorOverlay& overlay, const SceneCameraBasis& camera)
	{
		Entity selected = overlay.Selected; // a copy: Entity::GetScene is not const
		const bool drawSelected = overlay.SelectionOutline && selected.IsValid()
			&& selected.GetScene() == &scene && selected.HasComponent<WorldTransformComponent>();
		const bool scriptGizmos = overlay.ScriptGizmos != nullptr && overlay.ScriptGizmos->GetLines().empty() == false;
		if (drawSelected == false && overlay.Colliders == false && overlay.TerrainBrush == false && overlay.TerrainPath.empty()
			&& scriptGizmos == false)
		{
			return;
		}

		Renderer2D::BeginOverlay();

		// What scripts draw for the Editor view (OnDrawGizmos), over everything.
		if (scriptGizmos)
		{
			for (const Gizmos::LineItem& line : overlay.ScriptGizmos->GetLines())
			{
				Renderer2D::DrawLine3D(line.Start, line.End, line.Color, line.Thickness);
			}
		}

		// The Terrain tool's brush: a circle on the ground under the cursor, drawn over everything.
		if (overlay.TerrainBrush)
		{
			const uint32_t segments = 48;
			const glm::vec3 lift(0.0f, 0.0f, 0.01f);
			glm::vec3 previous = overlay.TerrainBrushCentre + lift + glm::vec3(overlay.TerrainBrushRadius, 0.0f, 0.0f);
			for (uint32_t i = 1; i <= segments; i++)
			{
				const float angle = glm::two_pi<float>() * (float)i / (float)segments;
				const glm::vec3 point = overlay.TerrainBrushCentre + lift + glm::vec3(std::cos(angle), std::sin(angle), 0.0f) * overlay.TerrainBrushRadius;
				Renderer2D::DrawLine3D(previous, point, overlay.TerrainBrushColor, 2.0f);
				previous = point;
			}
		}
		for (size_t i = 0; i < overlay.TerrainPath.size(); i++)
		{
			const glm::vec3 lift(0.0f, 0.0f, 0.02f);
			const glm::vec4 pathColor{ 1.0f, 0.85f, 0.2f, 1.0f };
			const glm::vec3 point = overlay.TerrainPath[i] + lift;
			// A small cross on each point, joined by a line.
			Renderer2D::DrawLine3D(point - glm::vec3(0.15f, 0.0f, 0.0f), point + glm::vec3(0.15f, 0.0f, 0.0f), pathColor, 2.0f);
			Renderer2D::DrawLine3D(point - glm::vec3(0.0f, 0.15f, 0.0f), point + glm::vec3(0.0f, 0.15f, 0.0f), pathColor, 2.0f);
			if (i > 0)
			{
				Renderer2D::DrawLine3D(overlay.TerrainPath[i - 1] + lift, point, pathColor, 2.0f);
			}
		}

		if (overlay.Colliders)
		{
			const glm::vec4 sensorColor{ 1.0f, 0.9f, 0.2f, 1.0f };
			const glm::vec4 staticColor{ 0.3f, 0.9f, 0.3f, 1.0f };
			const glm::vec4 dynamicColor{ 0.3f, 0.55f, 1.0f, 1.0f };
			const glm::vec4 kinematicColor{ 0.3f, 0.9f, 0.9f, 1.0f };

			const auto colorFor = [&](entt::entity entity, bool sensor)
			{
				if (sensor)
				{
					return sensorColor;
				}
				if (scene.m_Registry.has<Rigidbody2DComponent>(entity) == false)
				{
					return staticColor;
				}
				switch (scene.m_Registry.get<Rigidbody2DComponent>(entity).Type)
				{
				case Rigidbody2DType::Dynamic: return dynamicColor;
				case Rigidbody2DType::Kinematic: return kinematicColor;
				default: return staticColor;
				}
			};

			auto boxes = scene.m_Registry.view<WorldTransformComponent, BoxCollider2DComponent>();
			for (auto entity : boxes)
			{
				const auto [transform, box] = boxes.get<WorldTransformComponent, BoxCollider2DComponent>(entity);
				const glm::mat4 shape = transform.Matrix
					* glm::translate(glm::mat4(1.0f), glm::vec3(box.Offset, 0.0f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(box.Size, 1.0f));
				Renderer2D::DrawRect(shape, colorFor(entity, box.IsSensor), 1.5f);
			}

			auto circles = scene.m_Registry.view<WorldTransformComponent, CircleCollider2DComponent>();
			for (auto entity : circles)
			{
				const auto [transform, circle] = circles.get<WorldTransformComponent, CircleCollider2DComponent>(entity);
				const glm::vec3 centre = glm::vec3(transform.Matrix * glm::vec4(circle.Offset, 0.0f, 1.0f));
				const float scale = std::max(std::abs(transform.Scale.x), std::abs(transform.Scale.y));
				Renderer2D::DrawCircle(centre, circle.Radius * scale, colorFor(entity, circle.IsSensor), 1.5f);
			}
		}

		if (drawSelected)
		{
			const glm::vec4 selectionColor{ 1.0f, 0.65f, 0.1f, 1.0f };
			const float thickness = 2.5f;
			const WorldTransformComponent& transform = selected.GetComponent<WorldTransformComponent>();
			const glm::mat4 matrix = transform.Matrix;

			if (selected.HasComponent<SpriteRendererComponent>())
			{
				const SpriteRendererComponent& sprite = selected.GetComponent<SpriteRendererComponent>();
				const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
				const glm::mat4 quad = SceneSpriteRenderer::GetSpriteQuadTransform(matrix, loaded.get(), sprite.SpriteIndex);
				Renderer2D::DrawRect(SceneSpriteRenderer::ApplyFacing(quad, sprite.Facing, camera),
					selectionColor, thickness);
			}
			else
			{
				// Nothing drawn to outline: a small box on the entity (a point light also shows its reach).
				Renderer2D::DrawRect(transform.Translation, glm::vec2(0.6f), selectionColor, thickness);
				if (selected.HasComponent<Light2DComponent>())
				{
					const Light2DComponent& light = selected.GetComponent<Light2DComponent>();
					if (light.Type == Light2DType::Point)
					{
						Renderer2D::DrawCircle(transform.Translation, light.Radius, selectionColor, 1.5f);
					}
				}
			}
		}

		Renderer2D::EndOverlay();

		if (overlay.Colliders)
		{
			SubmitColliderDebugLines(scene);
		}
		if (drawSelected)
		{
			SubmitSelectionDebugLines(selected);
		}
	}

	// A circle of 3D lines about `centre` in the plane spanned by `u` and `v` (unit vectors).
	static void DrawCircle3D(const glm::vec3& centre, const glm::vec3& u, const glm::vec3& v, float radius, const glm::vec4& color,
		uint32_t segments = 48)
	{
		glm::vec3 previous = centre + u * radius;
		for (uint32_t i = 1; i <= segments; i++)
		{
			const float angle = glm::two_pi<float>() * (float)i / (float)segments;
			const glm::vec3 point = centre + (u * std::cos(angle) + v * std::sin(angle)) * radius;
			Renderer2D::DrawLine3D(previous, point, color);
			previous = point;
		}
	}

	// Two unit vectors at right angles to `axis` (unit length).
	static void MakePerpendicular(const glm::vec3& axis, glm::vec3& u, glm::vec3& v)
	{
		const glm::vec3 helper = std::abs(axis.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		u = glm::normalize(glm::cross(axis, helper));
		v = glm::cross(axis, u);
	}

	void SceneEditorRenderer::SubmitSelectionDebugLines(Entity selected)
	{
		const WorldTransformComponent& transform = selected.GetComponent<WorldTransformComponent>();
		const glm::vec4 boundsColor{ 1.0f, 0.65f, 0.1f, 1.0f };
		const glm::vec4 lightColor{ 1.0f, 0.9f, 0.4f, 0.9f };

		Renderer2D::BeginDebugLines();

		// A mesh: its bounding box, in its own space turned by the transform (12 edges).
		if (selected.HasComponent<MeshComponent>())
		{
			glm::vec3 low{ 0.0f };
			glm::vec3 high{ 0.0f };
			if (MeshBounds::GetLocal(selected.GetComponent<MeshComponent>(), low, high))
			{
				const glm::mat4 world = transform.Matrix;
				glm::vec3 corners[8];
				for (int i = 0; i < 8; i++)
				{
					const glm::vec3 local{ (i & 1) ? high.x : low.x, (i & 2) ? high.y : low.y, (i & 4) ? high.z : low.z };
					corners[i] = glm::vec3(world * glm::vec4(local, 1.0f));
				}
				const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 },
					{ 1, 5 }, { 2, 6 }, { 3, 7 } };
				for (const auto& edge : edges)
				{
					Renderer2D::DrawLine3D(corners[edge[0]], corners[edge[1]], boundsColor);
				}
			}
		}

		// A point or spot light: where it sits (its Height above the entity) and how far it reaches.
		if (selected.HasComponent<Light2DComponent>())
		{
			const Light2DComponent& light = selected.GetComponent<Light2DComponent>();
			const glm::vec3 position = transform.Translation + glm::vec3(0.0f, 0.0f, light.Height);
			if (light.Type == Light2DType::Point)
			{
				DrawCircle3D(position, glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), light.Radius, lightColor);
				DrawCircle3D(position, glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), light.Radius, lightColor);
				DrawCircle3D(position, glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), light.Radius, lightColor);
				if (light.Height != 0.0f)
				{
					Renderer2D::DrawLine3D(transform.Translation, position, lightColor);
				}
			}
			else if (light.Type == Light2DType::Spot)
			{
				const glm::vec3 direction = SceneRenderer::GetForwardDirection(transform);
				glm::vec3 u;
				glm::vec3 v;
				MakePerpendicular(direction, u, v);
				const float outer = glm::radians(std::clamp(std::max(light.OuterAngle, light.InnerAngle), 0.1f, 89.5f));
				const glm::vec3 centre = position + direction * (light.Radius * std::cos(outer));
				const float radius = light.Radius * std::sin(outer);
				DrawCircle3D(centre, u, v, radius, lightColor, 32);
				for (int i = 0; i < 4; i++)
				{
					const float angle = glm::half_pi<float>() * (float)i;
					Renderer2D::DrawLine3D(position, centre + (u * std::cos(angle) + v * std::sin(angle)) * radius, lightColor);
				}
			}
		}

		// The sun: an arrow from the entity along the way its light travels.
		if (selected.HasComponent<DirectionalLightComponent>())
		{
			const glm::vec3 direction = SceneRenderer::GetForwardDirection(transform);
			const glm::vec3 start = transform.Translation;
			const glm::vec3 end = start + direction * 3.0f;
			glm::vec3 u;
			glm::vec3 v;
			MakePerpendicular(direction, u, v);
			Renderer2D::DrawLine3D(start, end, lightColor, 2.0f);
			for (int i = 0; i < 4; i++)
			{
				const float angle = glm::half_pi<float>() * (float)i;
				Renderer2D::DrawLine3D(end, end - direction * 0.5f + (u * std::cos(angle) + v * std::sin(angle)) * 0.25f, lightColor, 2.0f);
			}
		}

		Renderer2D::EndDebugLines();
	}

	// One collider shape's edges. `world` places the shape's space (translation and rotation; the shape has the scale).
	static void DrawColliderShape(const glm::mat4& world, const ColliderShape& shape, const glm::vec4& color)
	{
		const auto toWorld = [&world](const glm::vec3& local) { return glm::vec3(world * glm::vec4(local, 1.0f)); };
		const glm::vec3 axisX = glm::normalize(glm::vec3(world[0]));
		const glm::vec3 axisY = glm::normalize(glm::vec3(world[1]));
		const glm::vec3 axisZ = glm::normalize(glm::vec3(world[2]));

		switch (shape.Type)
		{
			case ColliderShapeType::Box:
			{
				glm::vec3 corners[8];
				for (int i = 0; i < 8; i++)
				{
					const glm::vec3 sign{ (i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f };
					corners[i] = toWorld(shape.Centre + sign * shape.HalfExtents);
				}
				const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 },
					{ 1, 5 }, { 2, 6 }, { 3, 7 } };
				for (const auto& edge : edges)
				{
					Renderer2D::DrawLine3D(corners[edge[0]], corners[edge[1]], color);
				}
				break;
			}
			case ColliderShapeType::Sphere:
			{
				const glm::vec3 centre = toWorld(shape.Centre);
				DrawCircle3D(centre, axisX, axisY, shape.Radius, color, 32);
				DrawCircle3D(centre, axisX, axisZ, shape.Radius, color, 32);
				DrawCircle3D(centre, axisY, axisZ, shape.Radius, color, 32);
				break;
			}
			case ColliderShapeType::Capsule:
			case ColliderShapeType::Cylinder:
			{
				// Both stand along Z: a ring at each end of the straight part and four lines between them; a capsule
				// also shows its rounded ends as rings across.
				const glm::vec3 bottom = toWorld(shape.Centre - glm::vec3(0.0f, 0.0f, shape.HalfHeight));
				const glm::vec3 top = toWorld(shape.Centre + glm::vec3(0.0f, 0.0f, shape.HalfHeight));
				DrawCircle3D(bottom, axisX, axisY, shape.Radius, color, 24);
				DrawCircle3D(top, axisX, axisY, shape.Radius, color, 24);
				for (int i = 0; i < 4; i++)
				{
					const float angle = glm::half_pi<float>() * (float)i;
					const glm::vec3 side = (axisX * std::cos(angle) + axisY * std::sin(angle)) * shape.Radius;
					Renderer2D::DrawLine3D(bottom + side, top + side, color);
				}
				if (shape.Type == ColliderShapeType::Capsule)
				{
					DrawCircle3D(bottom, axisX, axisZ, shape.Radius, color, 24);
					DrawCircle3D(bottom, axisY, axisZ, shape.Radius, color, 24);
					DrawCircle3D(top, axisX, axisZ, shape.Radius, color, 24);
					DrawCircle3D(top, axisY, axisZ, shape.Radius, color, 24);
				}
				break;
			}
			case ColliderShapeType::ConvexHull:
			case ColliderShapeType::TriangleMesh:
			{
				// A model's collision node: its triangles' edges (shared edges drawn twice; cheap for proxies).
				for (size_t i = 0; i + 2 < shape.Indices.size(); i += 3)
				{
					const glm::vec3 a = toWorld(shape.Points[shape.Indices[i]]);
					const glm::vec3 b = toWorld(shape.Points[shape.Indices[i + 1]]);
					const glm::vec3 c = toWorld(shape.Points[shape.Indices[i + 2]]);
					Renderer2D::DrawLine3D(a, b, color);
					Renderer2D::DrawLine3D(b, c, color);
					Renderer2D::DrawLine3D(c, a, color);
				}
				break;
			}
			case ColliderShapeType::Prism:
			{
				// Two rings of the same size: each ring's loop and the edges joining them.
				const size_t count = shape.Points.size() / 2;
				for (size_t i = 0; i < count; i++)
				{
					const size_t next = (i + 1) % count;
					Renderer2D::DrawLine3D(toWorld(shape.Points[i]), toWorld(shape.Points[next]), color);
					Renderer2D::DrawLine3D(toWorld(shape.Points[count + i]), toWorld(shape.Points[count + next]), color);
					Renderer2D::DrawLine3D(toWorld(shape.Points[i]), toWorld(shape.Points[count + i]), color);
				}
				break;
			}
		}
	}

	void SceneEditorRenderer::SubmitColliderDebugLines(Scene& scene)
	{
		// The 2D outlines' colours: static green, game-moved cyan, triggers yellow; characters magenta.
		const glm::vec4 staticColor{ 0.3f, 0.9f, 0.3f, 1.0f };
		const glm::vec4 kinematicColor{ 0.3f, 0.9f, 0.9f, 1.0f };
		const glm::vec4 triggerColor{ 1.0f, 0.9f, 0.2f, 1.0f };
		const glm::vec4 characterColor{ 1.0f, 0.4f, 0.9f, 1.0f };

		entt::registry& registry = scene.m_Registry;
		std::vector<ColliderShape> shapes;
		bool begun = false;
		const auto begin = [&begun]()
		{
			if (begun == false)
			{
				Renderer2D::BeginDebugLines();
				begun = true;
			}
		};

		for (entt::entity entity : scene.GetEntityOrder())
		{
			if (registry.has<WorldTransformComponent>(entity) == false)
			{
				continue;
			}
			const WorldTransformComponent& transform = registry.get<WorldTransformComponent>(entity);
			const glm::mat4 world = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(transform.Rotation);

			// A character: its capsule on the entity's feet (the transform's scale doesn't size it).
			if (registry.has<CharacterBodyComponent>(entity))
			{
				const CharacterBodyComponent& body = registry.get<CharacterBodyComponent>(entity);
				ColliderShape capsule;
				capsule.Type = ColliderShapeType::Capsule;
				capsule.Radius = std::max(body.Radius, 0.05f);
				const float height = std::max(body.Height, 2.0f * capsule.Radius + 0.05f);
				capsule.HalfHeight = 0.5f * height - capsule.Radius;
				capsule.Centre = glm::vec3(0.0f, 0.0f, 0.5f * height);
				begin();
				DrawColliderShape(glm::translate(glm::mat4(1.0f), transform.Translation), capsule, characterColor);
				continue;
			}

			if (ColliderShapes::HasColliders(registry, entity) == false)
			{
				continue;
			}
			shapes.clear();
			ColliderShapes::Get(registry, entity, transform.Scale, shapes);
			const bool kinematic = registry.has<NativeScriptComponent>(entity);
			for (const ColliderShape& shape : shapes)
			{
				begin();
				DrawColliderShape(world, shape, shape.IsTrigger ? triggerColor : (kinematic ? kinematicColor : staticColor));
			}
		}

		if (begun)
		{
			Renderer2D::EndDebugLines();
		}
	}
}
