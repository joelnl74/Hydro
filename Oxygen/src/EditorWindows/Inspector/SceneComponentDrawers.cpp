#include "ComponentDrawers.h"
#include "InspectorUI.h"

#include <Hydro/Scene/Scene.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace Hydro
{
	// Physics bodies are made from the world transform, but under a rotated parent with non-uniform scale a shape
	// would need shear to match what is drawn, which it can't have.
	static bool HasNonUniformlyScaledParent(Entity entity)
	{
		Scene* scene = entity.GetScene();
		for (Entity parent = scene->GetParent(entity); (bool)parent; parent = scene->GetParent(parent))
		{
			if (parent.HasComponent<TransformComponent>())
			{
				const glm::vec3 scale = glm::abs(parent.GetComponent<TransformComponent>().Scale);
				if (std::abs(scale.x - scale.y) > 1e-4f || std::abs(scale.y - scale.z) > 1e-4f)
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool HasPhysicsBody(Entity entity)
	{
		return entity.HasComponent<Rigidbody2DComponent>() || entity.HasComponent<BoxCollider2DComponent>()
			|| entity.HasComponent<CircleCollider2DComponent>() || entity.HasComponent<BoxColliderComponent>()
			|| entity.HasComponent<SphereColliderComponent>() || entity.HasComponent<CapsuleColliderComponent>()
			|| entity.HasComponent<CylinderColliderComponent>() || entity.HasComponent<MeshColliderComponent>()
			|| entity.HasComponent<CharacterBodyComponent>();
	}

	static void DrawTransform(Entity entity, InspectorContext& context)
	{
		// A UI element is placed by its rect; a transform it may still carry is ignored, so it isn't shown.
		if (entity.HasComponent<UIRectComponent>())
		{
			return;
		}

		ComponentDrawers::Section<TransformComponent>("Transform", entity, context, [&](TransformComponent& transform)
		{
			InspectorUI::DrawVec3("Translation", transform.Translation);

			// Stored in radians, edited in degrees.
			glm::vec3 rotation = glm::degrees(transform.Rotation);
			if (InspectorUI::DrawVec3("Rotation", rotation, 0.0f))
			{
				transform.Rotation = glm::radians(rotation);
			}

			InspectorUI::DrawVec3("Scale", transform.Scale, 1.0f);

			// The values are local, relative to the parent.
			if (Entity parent = entity.GetScene()->GetParent(entity))
			{
				ImGui::TextDisabled("Relative to its parent, %s.", parent.GetName().c_str());
				if (HasPhysicsBody(entity) && HasNonUniformlyScaledParent(entity))
				{
					ImGui::TextWrapped("A parent has a non-uniform scale: when rotated, this entity's collider can't match what is drawn. Keep physics bodies under parents with an even scale.");
				}
			}
		});
	}

	static void DrawCamera(Entity entity, InspectorContext& context)
	{
		bool makePrimary = false;
		ComponentDrawers::Section<CameraComponent>("Camera", entity, context, [&](CameraComponent& cameraComponent)
		{
			SceneCamera& camera = cameraComponent.Camera;

			InspectorUI::Row("Primary",
				[&]() { makePrimary = ImGui::Checkbox("##Primary", &cameraComponent.Primary) && cameraComponent.Primary; });

			const char* projectionNames[] = { "Perspective", "Orthographic" };
			int projection = (int)camera.Projection;
			InspectorUI::Row("Projection", [&]()
			{
				if (ImGui::Combo("##Projection", &projection, projectionNames, 2))
				{
					camera.Projection = (ProjectionType)projection;
				}
			});

			if (camera.Projection == ProjectionType::Perspective)
			{
				InspectorUI::Row("Vertical FOV",
					[&]() { InspectorUI::InputFloat("##FOV", camera.PerspectiveFOV, "%.1f deg", 1.0f, 179.0f); });
				InspectorUI::Row("Near",
					[&]() { InspectorUI::InputFloat("##Near", camera.PerspectiveNear, "%.3f", 0.001f, camera.PerspectiveFar); });
				InspectorUI::Row("Far",
					[&]() { InspectorUI::InputFloat("##Far", camera.PerspectiveFar, "%.1f", camera.PerspectiveNear, 100000.0f); });
			}
			else
			{
				InspectorUI::Row("Size", [&]() { InspectorUI::InputFloat("##Size", camera.OrthographicSize, "%.2f", 0.01f, 10000.0f); });
				InspectorUI::Row("Near",
					[&]() { InspectorUI::InputFloat("##Near", camera.OrthographicNear, "%.2f", -10000.0f, camera.OrthographicFar); });
				InspectorUI::Row("Far",
					[&]() { InspectorUI::InputFloat("##Far", camera.OrthographicFar, "%.2f", camera.OrthographicNear, 10000.0f); });
			}
			InspectorUI::Row("Focus distance",
				[&]() { InspectorUI::InputFloat("##FocusDistance", camera.FocusDistance, "%.2f m", 0.0f, 100000.0f); });
			ImGui::TextDisabled("Depth of field focus along the view; 0 = where the view meets the ground. Camera scripts may set it.");
		});

		// Only one primary camera: ticking Primary here unticks it everywhere else.
		if (makePrimary)
		{
			auto cameras = entity.GetScene()->m_Registry.view<CameraComponent>();
			for (auto other : cameras)
			{
				if (other != (entt::entity)entity)
				{
					cameras.get<CameraComponent>(other).Primary = false;
				}
			}
		}
	}

	static void DrawLight2D(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<Light2DComponent>("Light 2D", entity, context, [&](Light2DComponent& light)
		{
			const char* typeNames[] = { "Point", "Global", "Spot" };
			int type = (int)light.Type;
			InspectorUI::Row("Type", [&]()
			{
				if (ImGui::Combo("##LightType", &type, typeNames, 3))
				{
					light.Type = (Light2DType)type;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Spot lights shine only under forward lighting (Render Settings > Lighting: Forward).\nThey point along the entity's -Z, turned by its rotation.");
				}
			});

			InspectorUI::Row("Color", [&]() { ImGui::ColorEdit3("##LightColor", glm::value_ptr(light.Color)); });
			InspectorUI::Row("Intensity", [&]() { InspectorUI::InputFloat("##LightIntensity", light.Intensity, "%.2f", 0.0f, 100.0f); });

			if (light.Type == Light2DType::Point || light.Type == Light2DType::Spot)
			{
				InspectorUI::Row(light.Type == Light2DType::Spot ? "Range" : "Radius",
					[&]() { InspectorUI::InputFloat("##LightRadius", light.Radius, "%.2f", 0.01f, 10000.0f); });
				InspectorUI::Row("Height", [&]()
				{
					InspectorUI::InputFloat("##LightHeight", light.Height, "%.2f", 0.0f, 10000.0f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How far above its position the light is (a lantern's glass).\nForward lighting: that far up along Z. Light buffer: that far up the screen top-down, up the standing sprite with a tilted camera.");
					}
				});
				if (light.Type == Light2DType::Spot)
				{
					InspectorUI::Row("Inner Angle",
						[&]() { InspectorUI::InputFloat("##LightInner", light.InnerAngle, "%.1f deg", 0.0f, 89.0f); });
					InspectorUI::Row("Outer Angle",
						[&]() { InspectorUI::InputFloat("##LightOuter", light.OuterAngle, "%.1f deg", light.InnerAngle, 89.5f); });
				}
			}
			else
			{
				ImGui::TextDisabled("Lights everything equally: the ambient color.");
			}
		});
	}

	static void DrawDirectionalLight(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<DirectionalLightComponent>("Directional Light", entity, context, [&](DirectionalLightComponent& light)
		{
			InspectorUI::Row("Color", [&]() { ImGui::ColorEdit3("##SunColor", glm::value_ptr(light.Color)); });
			InspectorUI::Row("Intensity", [&]() { InspectorUI::InputFloat("##SunIntensity", light.Intensity, "%.2f", 0.0f, 100.0f); });
			InspectorUI::Row("Main Light", [&]()
			{
				ImGui::Checkbox("##SunMain", &light.IsMainLight);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The sun of the scene's forward lighting: the one marked, else the first.\nIt shines along the entity's -Z, turned by its rotation.");
				}
			});
			InspectorUI::Row("Cast Shadows", [&]()
			{
				ImGui::Checkbox("##SunCastShadows", &light.CastShadows);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The main light's shadow map, when the render features have Shadows = Sun.");
				}
			});
			ImGui::BeginDisabled(light.CastShadows == false);
			InspectorUI::Row("Shadow Strength", [&]()
			{
				InspectorUI::InputFloat("##SunShadowStrength", light.ShadowStrength, "%.2f", 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("1: no direct sun in the shadow (ambient and lamps still reach it). Lower: lighter shadows.");
				}
			});
			InspectorUI::Row("Shadow Distance", [&]()
			{
				InspectorUI::InputFloat("##SunShadowDistance", light.ShadowDistance, "%.1f m", 1.0f, 500.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Metres around what the camera looks at that get shadows (the map's half width).\nSmaller: sharper shadows over less of the scene.");
				}
			});
			InspectorUI::Row("Shadow Bias", [&]()
			{
				InspectorUI::InputFloat("##SunShadowBias", light.ShadowBias, "%.2f", 0.0f, 10.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How far surfaces are pushed out of their own shadow. Raise it for speckled\n(acne) surfaces, lower it when shadows float off the objects casting them.");
				}
			});
			ImGui::EndDisabled();
			ImGui::TextDisabled("Shines only under forward lighting (Render Settings > Lighting: Forward).");
		});
	}

	// Only the first camera in a scene is primary by default.
	static void AddCamera(Entity entity)
	{
		const bool firstCamera = (bool)entity.GetScene()->GetPrimaryCameraEntity() == false;
		entity.AddComponent<CameraComponent>().Primary = firstCamera;
		if (entity.HasComponent<TransformComponent>() == false)
		{
			entity.AddComponent<TransformComponent>();
		}
	}

	// Pointing down and a little forward, so a new sun lights the ground at once.
	static void AddDirectionalLight(Entity entity)
	{
		entity.AddComponent<DirectionalLightComponent>();
		if (entity.HasComponent<TransformComponent>() == false)
		{
			entity.AddComponent<TransformComponent>().Rotation = glm::vec3(glm::radians(-35.0f), 0.0f, 0.0f);
		}
	}

	void ComponentDrawers::AddSceneDrawers(std::vector<ComponentDrawer>& drawers)
	{
		drawers.push_back(Make<TransformComponent>("Transform", &DrawTransform, &AddOnly<TransformComponent>));
		drawers.push_back(Make<CameraComponent>("Camera", &DrawCamera, &AddCamera));
		// A point light is placed by the entity's transform.
		drawers.push_back(Make<Light2DComponent>("Light 2D", &DrawLight2D, &AddWithTransform<Light2DComponent>));
		drawers.push_back(Make<DirectionalLightComponent>("Directional Light", &DrawDirectionalLight, &AddDirectionalLight));
	}
}
