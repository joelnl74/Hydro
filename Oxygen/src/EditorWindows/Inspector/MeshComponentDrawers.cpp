#include "ComponentDrawers.h"
#include "InspectorUI.h"
#include "MaterialEditor.h"
#include "EditorWindows/TerrainTool.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Mesh.h>

#include <algorithm>
#include <string>

namespace Hydro
{
	static void DrawMesh(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<MeshComponent>("Mesh", entity, context, [&](MeshComponent& mesh)
		{
			const Ref<Mesh> model = (uint64_t)mesh.Model != 0 ? AssetManager::GetAsset<Mesh>(mesh.Model) : nullptr;
			InspectorUI::Row("Model", [&]()
			{
				const std::string detail = model != nullptr
					? std::to_string(model->GetSubMeshes().size()) + " sub-meshes, " + std::to_string(model->GetIndexCount() / 3) + " triangles, "
						+ std::to_string(model->GetMaterialSlotCount()) + " material slots"
					: ((uint64_t)mesh.Model != 0 ? "The model is missing or couldn't be imported (see the console)."
						: "Empty: a built-in shape.");
				const AssetPickerResult result = AssetPicker::DrawField("##MeshModel", mesh.Model, AssetType::Mesh, detail);
				if (result.Changed == false)
				{
					return;
				}
				if ((uint64_t)result.Handle == 0)
				{
					mesh.Model = 0;
					return;
				}
				// Importing logs why a model can't be used; a handle that doesn't import is not taken.
				const Ref<Mesh> picked = AssetManager::GetAsset<Mesh>(result.Handle);
				if (picked != nullptr)
				{
					mesh.Model = result.Handle;
					// Its own materials until the slots are made here.
					mesh.Materials.clear();
				}
			});
			if ((uint64_t)mesh.Model != 0)
			{
				ImGui::Separator();
				if (mesh.Materials.empty())
				{
					ImGui::TextDisabled("Uses the model's materials (the .hymat files its import made).");
					if (model != nullptr && ImGui::Button("Choose materials here"))
					{
						MaterialEditor::AssignModelMaterials(mesh, *model);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Gives this mesh a slot per material, starting with the model's, so one slot can use another material.");
					}
				}
				else
				{
					context.Materials->DrawMeshSlots(entity, mesh, model.get());
					if (ImGui::Button("Use the model's materials"))
					{
						mesh.Materials.clear();
					}
				}
				InspectorUI::Row("Cast shadows", [&]() { ImGui::Checkbox("##MeshCastShadows", &mesh.CastShadows); });
				InspectorUI::Row("Opacity", [&]() { InspectorUI::InputFloat("##MeshOpacity", mesh.Opacity, "%.2f", 0.0f, 1.0f); });
				ImGui::TextDisabled("Drawn with Depth = Shared and Meshes on (Render Settings > Rendering).");
				return;
			}

			InspectorUI::Row("Shape", [&]()
			{
				const char* shapeNames[] = { "Box", "Plane", "Ramp", "Stairs", "Cylinder" };
				int shape = (int)mesh.Shape;
				if (ImGui::Combo("##MeshShape", &shape, shapeNames, 5))
				{
					mesh.Shape = (MeshShape)shape;
				}
			});
			InspectorUI::DrawVec3("Size", mesh.Size, 1.0f);
			if (mesh.Shape == MeshShape::Stairs)
			{
				InspectorUI::Row("Steps", [&]()
				{
					int steps = (int)mesh.Steps;
					if (ImGui::InputInt("##MeshSteps", &steps))
					{
						mesh.Steps = (uint32_t)std::clamp(steps, 1, 64);
					}
				});
			}
			if (mesh.Shape == MeshShape::Cylinder)
			{
				InspectorUI::Row("Segments", [&]()
				{
					int segments = (int)mesh.Segments;
					if (ImGui::InputInt("##MeshSegments", &segments))
					{
						mesh.Segments = (uint32_t)std::clamp(segments, 3, 128);
					}
				});
			}
			ImGui::TextDisabled("Metres, Z up, standing on the entity's position.");

			if (mesh.Materials.empty())
			{
				mesh.Materials.push_back(MeshMaterialSlot{});
			}
			context.Materials->DrawMeshSlots(entity, mesh, nullptr);
			InspectorUI::Row("Cast shadows", [&]() { ImGui::Checkbox("##MeshCastShadows", &mesh.CastShadows); });
			InspectorUI::Row("Opacity", [&]() { InspectorUI::InputFloat("##MeshOpacity", mesh.Opacity, "%.2f", 0.0f, 1.0f); });
			ImGui::TextDisabled("Drawn with Depth = Shared and Meshes on (Render Settings > Rendering).");
		});
	}

	static void DrawTerrain(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<TerrainComponent>("Terrain", entity, context, [&](TerrainComponent& terrain)
		{
			InspectorUI::Row("Cast Shadows", [&]() { ImGui::Checkbox("##TerrainCastShadows", &terrain.CastShadows); });
			InspectorUI::PhysicsLayerRow(terrain.Layer);
			if (context.Terrain != nullptr)
			{
				context.Terrain->DrawInspector(entity, terrain, context.Playing);
			}
		});
	}

	void ComponentDrawers::AddMeshDrawers(std::vector<ComponentDrawer>& drawers)
	{
		drawers.push_back(Make<MeshComponent>("Mesh", &DrawMesh, &AddWithTransform<MeshComponent>));
		// Its corner (0, 0) at the entity's position; the section then offers Create Terrain.
		drawers.push_back(Make<TerrainComponent>("Terrain", &DrawTerrain, &AddWithTransform<TerrainComponent>));
	}
}
