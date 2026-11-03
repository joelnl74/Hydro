#include "EditorViewport.h"
#include "CaptureControls.h"

#include <Hydro/Core/Input.h>
#include <Hydro/Core/Time.h>
#include <Hydro/Events/MouseEvents.h>
#include <Hydro/Scene/Components/Components.h>

#include <ImGuizmo.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Hydro
{
	EditorViewport::EditorViewport(EditorContext& context)
		: m_Context(context)
	{
	}

	void EditorViewport::Init()
	{
		RenderViewSpecification specification;
		specification.DebugName = "Editor";
		specification.EditorView = true;
		m_View = CreateScope<RenderView>(specification);
	}

	void EditorViewport::Shutdown()
	{
		m_View.reset();
	}

	void EditorViewport::OnUpdate(float deltaTime, SceneRenderer& renderer, bool covered)
	{
		m_Camera.SetSettings(m_Context.Settings.Camera);
		m_Camera.Update(deltaTime, m_Visible && m_Hovered && m_GizmoUsing == false);
		if (m_Visible == false || covered)
		{
			return;
		}

		SceneRenderer::EditorOverlay overlay;
		overlay.Selected = m_Context.Selection;
		overlay.SelectionOutline = m_ShowSelectionOutline;
		overlay.Colliders = m_ShowColliders;
		overlay.PostProcessing = m_Context.Settings.EditorViewPostProcessing;
		overlay.Fog = m_Context.Settings.EditorViewFog;
		m_ScriptGizmos.Clear();
		if (m_ShowScriptGizmos && m_Context.ActiveScene != nullptr)
		{
			CollectScriptGizmos(*m_Context.ActiveScene, overlay.Selected, m_ScriptGizmos);
			overlay.ScriptGizmos = &m_ScriptGizmos;
		}
		m_Context.Terrain.FillOverlay(overlay);
		renderer.RenderEditor(*m_View, m_Camera, overlay);
	}

	void EditorViewport::OnEvent(Event& event)
	{
		if (event.GetEventType() == EventType::MouseScrolled && m_Visible && m_Hovered)
		{
			m_Camera.Zoom(static_cast<MouseScrolledEvent&>(event).GetYOffset());
			event.Handled = true;
		}
	}


	void EditorViewport::OnImGuiRender()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		m_Visible = ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_MenuBar);
		m_Hovered = false;
		m_GizmoHovered = false;
		m_GizmoUsing = false;

		if (m_Visible)
		{
			if (ImGui::BeginMenuBar())
			{
				DrawCameraMenu();
				DrawGizmoMenu();

				if (ImGui::BeginMenu("Overlays"))
				{
					ImGui::MenuItem("Selection Outline", nullptr, &m_ShowSelectionOutline);
					ImGui::MenuItem("Collider Outlines", nullptr, &m_ShowColliders);
					ImGui::MenuItem("Script Gizmos", nullptr, &m_ShowScriptGizmos);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("What scripts draw here (OnDrawGizmos): routes and their waypoint numbers, ...");
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Focus Selected", "F", false, (bool)m_Context.Selection))
					{
						FocusEntity(m_Context.Selection);
					}

					ImGui::EndMenu();
				}

				// The scene's look in this view (off keeps it sharp to edit in), kept until the Editor Settings are saved.
				if (ImGui::BeginMenu("Effects"))
				{
					ImGui::MenuItem("Post-Processing", nullptr, &m_Context.Settings.EditorViewPostProcessing);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Depth of field, bloom and the vignette, as in the Game view");
					}
					ImGui::MenuItem("Fog", nullptr, &m_Context.Settings.EditorViewFog);
					ImGui::EndMenu();
				}

				CaptureControls::DrawMenu(m_Context.Captures);

				ImGui::EndMenuBar();
			}

			const ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.x >= 1.0f && size.y >= 1.0f)
			{
				Resize((uint32_t)size.x, (uint32_t)size.y);
			}

			// Camera projections flip Y for Vulkan, so the image is shown the right way up as is.
			ImGui::Image((ImTextureID)m_View->GetImGuiTexture(), size);
			m_Hovered = ImGui::IsItemHovered();
			const ImVec2 imageMin = ImGui::GetItemRectMin();
			DrawScriptGizmoLabels(imageMin, size);
			DrawCameraHint(imageMin);
			// The Terrain tool first: while it paints, the left button is its own (no gizmo, no click-to-select).
			m_TerrainToolOwnsMouse = m_Context.Terrain.UpdateViewport(m_Context.Selection, m_Camera,
				glm::vec2(imageMin.x, imageMin.y),
				glm::vec2(size.x, size.y), m_Hovered, m_Context.Playing, Time::GetDeltaTime(), m_Context.Commands);
			if (m_TerrainToolOwnsMouse == false)
			{
				// The gizmo first: a click on one of its handles is not a click to select.
				DrawTransformGizmo(imageMin, size);
				UpdateClickSelection(imageMin, size);
			}
			UpdateGizmoKeys();
			m_Context.AcceptAssetDrop();

			// F: look at the selected entity (edit mode only; play mode scripts may use F).
			const bool focusKey = Input::IsKeyPressed(KeyCode::F);
			if (focusKey && m_FocusKeyDown == false && m_Hovered && m_Context.Playing == false && ImGui::GetIO().WantTextInput == false)
			{
				FocusEntity(m_Context.Selection);
			}
			m_FocusKeyDown = focusKey;
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorViewport::DrawCameraMenu()
	{
		const bool orthographic = m_Camera.GetProjectionType() == ProjectionType::Orthographic;

		if (ImGui::BeginMenu("Camera"))
		{
			if (ImGui::MenuItem("2D", "Orthographic, top-down", orthographic))
			{
				m_Camera.SetProjectionType(ProjectionType::Orthographic);
			}
			if (ImGui::MenuItem("3D", "Perspective", orthographic == false))
			{
				m_Camera.SetProjectionType(ProjectionType::Perspective);
			}

			ImGui::Separator();
			// The standard views turn the camera around what it looks at (and switch to 3D).
			struct ViewItem
			{
				const char* Label;
				EditorCameraView View;
			};
			static const ViewItem views[] = {
				{ "Top", EditorCameraView::Top }, { "Diorama (50 degrees)", EditorCameraView::Diorama },
				{ "Front (looking north)", EditorCameraView::Front }, { "Back (looking south)", EditorCameraView::Back },
				{ "Left (looking east)", EditorCameraView::Left }, { "Right (looking west)", EditorCameraView::Right }
			};
			for (const ViewItem& item : views)
			{
				if (ImGui::MenuItem(item.Label))
				{
					m_Camera.SetView(item.View);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Reset View"))
			{
				m_Camera.Reset();
			}

			ImGui::Separator();
			ImGui::TextDisabled("Alt+LMB orbit, MMB pan, wheel / Alt+RMB zoom");
			ImGui::TextDisabled("3D: hold RMB to fly: mouse look, WASD move, Q/E down/up,");
			ImGui::TextDisabled("Shift faster, wheel changes the speed");

			ImGui::EndMenu();
		}

		// A one-click switch next to the menu: shows the mode you're in, switches to the other.
		if (ImGui::MenuItem(orthographic ? "2D" : "3D"))
		{
			m_Camera.SetProjectionType(orthographic ? ProjectionType::Perspective : ProjectionType::Orthographic);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(orthographic ? "2D view (orthographic, top-down). Click for 3D" : "3D view (perspective). Click for 2D");
		}
	}

	void EditorViewport::DrawScriptGizmoLabels(const ImVec2& imageMin, const ImVec2& imageSize)
	{
		if (m_ShowScriptGizmos == false || m_ScriptGizmos.GetLabels().empty())
		{
			return;
		}
		// The camera the view was drawn with this frame (zero-to-one depth; the Y flip puts the image's top at -1).
		const glm::mat4 viewProjection = m_Camera.GetViewProjection();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true);
		for (const Gizmos::LabelItem& label : m_ScriptGizmos.GetLabels())
		{
			const glm::vec4 clip = viewProjection * glm::vec4(label.Position, 1.0f);
			if (clip.w <= 1e-4f)
			{
				continue; // behind the camera
			}
			const glm::vec3 ndc = glm::vec3(clip) / clip.w;
			if (ndc.z < 0.0f || ndc.z > 1.0f)
			{
				continue;
			}
			const ImVec2 textSize = ImGui::CalcTextSize(label.Text.c_str());
			const ImVec2 position{ imageMin.x + (ndc.x * 0.5f + 0.5f) * imageSize.x - textSize.x * 0.5f,
				imageMin.y + (ndc.y * 0.5f + 0.5f) * imageSize.y - textSize.y * 0.5f };
			const ImU32 shadow = IM_COL32(0, 0, 0, (int)(200.0f * label.Color.a));
			for (const ImVec2 offset : { ImVec2(-1.0f, 0.0f), ImVec2(1.0f, 0.0f), ImVec2(0.0f, -1.0f), ImVec2(0.0f, 1.0f) })
			{
				drawList->AddText(ImVec2(position.x + offset.x, position.y + offset.y), shadow, label.Text.c_str());
			}
			drawList->AddText(position, ImGui::GetColorU32(ImVec4(label.Color.r, label.Color.g, label.Color.b, label.Color.a)),
				label.Text.c_str());
		}
		drawList->PopClipRect();
	}

	void EditorViewport::DrawCameraHint(const ImVec2& imageMin)
	{
		// While flying: the speed in the view's corner, so the wheel's change is visible.
		if (m_Camera.IsFlying() == false)
		{
			return;
		}

		char text[64];
		std::snprintf(text, sizeof(text), "Fly %.1f m/s", m_Camera.GetFlySpeed());
		const ImVec2 position{ imageMin.x + 8.0f, imageMin.y + 6.0f };
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddText(ImVec2(position.x + 1.0f, position.y + 1.0f), ImGui::GetColorU32(ImGuiCol_WindowBg), text);
		drawList->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), text);
	}

	void EditorViewport::UpdateClickSelection(const ImVec2& imageMin, const ImVec2& imageSize)
	{
		// The answer to an earlier click: the entity drawn under it, or nothing (clears the selection).
		int32_t picked = -1;
		if (m_View->TakeEntityIDResult(picked) && m_Context.ActiveScene != nullptr)
		{
			const entt::entity handle = (entt::entity)(uint32_t)picked;
			if (picked >= 0 && m_Context.ActiveScene->m_Registry.valid(handle))
			{
				m_Context.Selection = Entity(handle, m_Context.ActiveScene.get());
			}
			else
			{
				m_Context.Selection = {};
			}
		}

		// A click is a press and release in the image without moving, and without Alt (camera controls).
		const ImGuiIO& io = ImGui::GetIO();
		const bool alt = io.KeyAlt;
		if (m_Hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && alt == false && m_GizmoHovered == false
			&& ImGui::GetDragDropPayload() == nullptr)
		{
			m_ClickPending = true;
			m_ClickStart = io.MousePos;
		}
		if (m_ClickPending == false || ImGui::IsMouseReleased(ImGuiMouseButton_Left) == false)
		{
			return;
		}
		m_ClickPending = false;

		const float dx = io.MousePos.x - m_ClickStart.x;
		const float dy = io.MousePos.y - m_ClickStart.y;
		if (m_Hovered == false || alt || dx * dx + dy * dy > 16.0f || imageSize.x < 1.0f || imageSize.y < 1.0f)
		{
			return;
		}

		// ImGui pixels to the view's own (the same unless the view hasn't caught up with a resize yet).
		const float u = (io.MousePos.x - imageMin.x) / imageSize.x;
		const float v = (io.MousePos.y - imageMin.y) / imageSize.y;
		if (u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f)
		{
			return;
		}
		const uint32_t x = std::min((uint32_t)(u * (float)m_View->GetWidth()), m_View->GetWidth() - 1);
		const uint32_t y = std::min((uint32_t)(v * (float)m_View->GetHeight()), m_View->GetHeight() - 1);
		m_View->RequestEntityIDReadback(x, y);
	}

	void EditorViewport::FocusEntity(Entity entity)
	{
		if (entity.IsValid() == false || entity.HasComponent<TransformComponent>() == false)
		{
			return;
		}

		// Where it is in the world (a child's transform is relative to its parent).
		const glm::mat4 world = entity.GetScene()->GetWorldTransform(entity);
		const float extent = std::max({ glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])), 1.0f });
		m_Camera.Frame(glm::vec3(world[3]), extent);
	}

	// Each angle moved by whole turns to the one nearest `previous` (radians), so a rotation handed back as -90
	// degrees stays 270 when it was 265.
	static glm::vec3 NearestAngles(glm::vec3 angles, const glm::vec3& previous)
	{
		const float turn = glm::two_pi<float>();
		for (int axis = 0; axis < 3; axis++)
		{
			angles[axis] += turn * std::round((previous[axis] - angles[axis]) / turn);
		}
		return angles;
	}

	void EditorViewport::DrawTransformGizmo(const ImVec2& imageMin, const ImVec2& imageSize)
	{
		// Edits go to the editor scene, so there is no gizmo while playing.
		Entity selected = m_Context.Selection;
		if (m_GizmoOperation == GizmoOperation::None || m_Context.Playing || m_Context.ActiveScene == nullptr || selected.IsValid() == false
			|| selected.HasComponent<TransformComponent>() == false || imageSize.x < 1.0f || imageSize.y < 1.0f)
		{
			return;
		}

		ImGuizmo::SetOrthographic(m_Camera.GetProjectionType() == ProjectionType::Orthographic);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);
		// Alt + left drag belongs to the camera, and so does the mouse while a camera drag lasts.
		ImGuizmo::Enable(ImGui::GetIO().KeyAlt == false && m_Camera.IsDragging() == false);
		ImGuizmo::GetStyle().Colors[ImGuizmo::SELECTION] = m_Context.Settings.Style.GetHighlightColor();

		// ImGuizmo uses OpenGL's Y-up clip space: take out the Vulkan Y flip. The zero-to-one depth doesn't matter to it.
		glm::mat4 projection = m_Camera.GetProjection();
		projection[1][1] *= -1.0f;
		const glm::mat4& view = m_Camera.GetView();

		ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
		float snapStep = m_Context.Settings.Gizmo.TranslateSnap;
		switch (m_GizmoOperation)
		{
		case GizmoOperation::Rotate:
			operation = ImGuizmo::ROTATE;
			snapStep = m_Context.Settings.Gizmo.RotateSnap;
			break;
		case GizmoOperation::Scale:
			operation = ImGuizmo::SCALE;
			snapStep = m_Context.Settings.Gizmo.ScaleSnap;
			break;
		default:
			break;
		}
		// ImGuizmo scales along the entity's own axes only.
		const ImGuizmo::MODE mode = (m_GizmoLocal || m_GizmoOperation == GizmoOperation::Scale) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
		const float snap[3] = { snapStep, snapStep, snapStep };
		const bool snapping = ImGui::GetIO().KeyCtrl;

		// The gizmo sits on the entity's world transform; the result goes back into the parent's space so the write-back
		// below works on the local transform.
		Scene& scene = *selected.GetScene();
		const glm::mat4 parentWorld = scene.GetWorldTransform(scene.GetParent(selected));
		TransformComponent& transform = selected.GetComponent<TransformComponent>();
		glm::mat4 matrix = parentWorld * transform.GetTransform();
		const bool changed = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operation, mode,
			glm::value_ptr(matrix), nullptr, snapping ? snap : nullptr);

		m_GizmoUsing = ImGuizmo::IsUsing();
		m_GizmoHovered = m_GizmoUsing || ImGuizmo::IsOver();
		if (changed == false || m_GizmoUsing == false)
		{
			return;
		}
		if (std::abs(glm::determinant(glm::mat3(parentWorld))) < 1e-12f)
		{
			return; // a parent with a zero scale: there is no local transform to go back to
		}
		matrix = glm::inverse(parentWorld) * matrix;

		// Only the dragged part is written back, so the others keep their exact values. Column i of the matrix is the
		// rotation's axis i times Scale[i], signs included (a mirrored sprite stays mirrored).
		switch (m_GizmoOperation)
		{
		case GizmoOperation::Translate:
			transform.Translation = glm::vec3(matrix[3]);
			break;
		case GizmoOperation::Rotate:
		{
			const glm::vec3& scale = transform.Scale;
			if (scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f)
			{
				break;
			}
			const glm::mat3 rotation(glm::vec3(matrix[0]) / scale.x, glm::vec3(matrix[1]) / scale.y, glm::vec3(matrix[2]) / scale.z);
			transform.Rotation = NearestAngles(glm::eulerAngles(glm::normalize(glm::quat_cast(rotation))), transform.Rotation);
			break;
		}
		case GizmoOperation::Scale:
			for (int axis = 0; axis < 3; axis++)
			{
				const float length = glm::length(glm::vec3(matrix[axis]));
				transform.Scale[axis] = transform.Scale[axis] < 0.0f ? -length : length;
			}
			break;
		default:
			break;
		}
	}

	void EditorViewport::DrawGizmoMenu()
	{
		// One-click switches; the active tool shows selected, and a second click hides the gizmo.
		struct GizmoItem
		{
			const char* Label;
			const char* Tooltip;
			GizmoOperation Operation;
		};
		static const GizmoItem items[] = {
			{ "Move", "Move the selected entity (W). Ctrl snaps.", GizmoOperation::Translate },
			{ "Rotate", "Rotate the selected entity (E). Ctrl snaps.", GizmoOperation::Rotate },
			{ "Scale", "Scale the selected entity (R). Ctrl snaps.", GizmoOperation::Scale }
		};

		ImGui::Separator();
		ImGui::BeginDisabled(m_Context.Playing);
		for (const GizmoItem& item : items)
		{
			const bool active = m_GizmoOperation == item.Operation;
			if (ImGui::MenuItem(item.Label, nullptr, active))
			{
				m_GizmoOperation = active ? GizmoOperation::None : item.Operation;
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", m_Context.Playing ? "The gizmo edits the editor scene: stop playing to use it" : item.Tooltip);
			}
		}

		if (ImGui::MenuItem(m_GizmoLocal ? "Local" : "World"))
		{
			m_GizmoLocal = m_GizmoLocal == false;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip(m_GizmoLocal ? "Gizmo along the entity's own axes. Click or T for the world's" : "Gizmo along the world's axes. Click or T for the entity's own (Scale always uses its own)");
		}
		ImGui::EndDisabled();
		ImGui::Separator();
	}

	void EditorViewport::UpdateGizmoKeys()
	{
		// Ctrl+key belongs to menu shortcuts; while the camera drags (flying), W/E/Q move it.
		const ImGuiIO& io = ImGui::GetIO();
		if (m_Hovered == false || m_Context.Playing || io.WantTextInput || io.KeyCtrl || m_Camera.IsDragging() || m_GizmoUsing)
		{
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
		{
			m_GizmoOperation = GizmoOperation::None;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
		{
			m_GizmoOperation = GizmoOperation::Translate;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
		{
			m_GizmoOperation = GizmoOperation::Rotate;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
		{
			m_GizmoOperation = GizmoOperation::Scale;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_T, false))
		{
			m_GizmoLocal = m_GizmoLocal == false;
		}
	}

	void EditorViewport::Resize(uint32_t width, uint32_t height)
	{
		if (m_Context.Captures.Editor)
		{
			if (m_Context.Captures.FixedSize)
			{
				width = CaptureRequests::FixedWidth;
				height = CaptureRequests::FixedHeight;
			}
			m_View->RequestCapture(CaptureControls::MakePath(m_Context.ActiveScene.get(), "Editor"));
			m_Context.Captures.Editor = false;
		}

		m_View->SetSize(width, height);
		m_Camera.SetViewportSize((float)width, (float)height);
	}
}
