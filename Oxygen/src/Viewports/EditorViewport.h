#pragma once
#include "EditorContext.h"

#include <Hydro/Core/Base.h>
#include <Hydro/Events/Event.h>
#include <Hydro/Renderer/EditorCamera.h>
#include <Hydro/Renderer/RenderView.h>
#include <Hydro/Renderer/SceneRenderer.h>
#include <Hydro/Scene/Scripting/Gizmos.h>

#include <imgui/imgui.h>

#include <cstdint>

namespace Hydro
{
	// The Editor window: the active scene through the editor camera, which it controls, with the selection outline,
	// colliders and script gizmos over it; click-to-select, the transform gizmo and the Terrain tool's brush.
	class EditorViewport
	{
	public:
		explicit EditorViewport(EditorContext& context);

		void Init();
		// While the GPU is idle (the application waits before detaching layers).
		void Shutdown();

		// The camera updates every frame so a drag never jumps, but only takes new input over the window. `covered`:
		// game full screen hides the window, so it isn't rendered.
		void OnUpdate(float deltaTime, SceneRenderer& renderer, bool covered);
		void OnImGuiRender();
		// The wheel zooms while the pointer is over the window.
		void OnEvent(Event& event);

		// Moves the camera to the entity and zooms to frame it (the selection outline shows which).
		void FocusEntity(Entity entity);
		// The scene changed (opened, Play, Stop): answers to earlier clicks are thrown away.
		void OnSceneChanged() { m_View->InvalidateEntityIDRequests(); }
		EditorCamera& GetCamera() { return m_Camera; }

	private:
		// The Camera menu (2D / 3D, the standard views, the controls) and the 2D/3D switch.
		void DrawCameraMenu();
		// Over the image: the fly speed while the camera flies.
		void DrawCameraHint(const ImVec2& imageMin);
		// The script gizmos' labels (OnDrawGizmos), projected onto the image.
		void DrawScriptGizmoLabels(const ImVec2& imageMin, const ImVec2& imageSize);
		// The transform gizmo on the selected entity, in edit mode only. Holding Ctrl snaps to EditorSettings::Gizmo.
		void DrawTransformGizmo(const ImVec2& imageMin, const ImVec2& imageSize);
		// The gizmo's switches in the menu bar: Move / Rotate / Scale and Local / World.
		void DrawGizmoMenu();
		// W move, E rotate, R scale, Q none, T local/world: over the window, in edit mode, while the camera isn't
		// dragging and nothing takes text.
		void UpdateGizmoKeys();
		// A left click in the image asks the view for the entity under it; the answer arrives a frame or two later.
		void UpdateClickSelection(const ImVec2& imageMin, const ImVec2& imageSize);
		// Sizes the view and the camera's viewport to the window; a pending capture is requested here.
		void Resize(uint32_t width, uint32_t height);

	private:
		enum class GizmoOperation : uint8_t { None, Translate, Rotate, Scale };

		EditorContext& m_Context;
		Scope<RenderView> m_View;
		EditorCamera m_Camera;
		bool m_Visible = false;
		bool m_Hovered = false;
		// Overlays (this session only): outline the selected entity / every collider.
		bool m_ShowSelectionOutline = true;
		bool m_ShowColliders = false;
		// What scripts draw for this view (OnDrawGizmos), collected every frame while shown.
		bool m_ShowScriptGizmos = true;
		Gizmos m_ScriptGizmos;
		bool m_FocusKeyDown = false; // F pressed last frame, so holding it focuses once
		// Transform gizmo (this session only). None hides it.
		GizmoOperation m_GizmoOperation = GizmoOperation::Translate;
		bool m_GizmoLocal = false;   // the entity's own axes instead of the world's (scaling always uses its own)
		bool m_GizmoHovered = false; // the mouse is over a handle or drags one: a click there doesn't select
		bool m_GizmoUsing = false;   // a handle is being dragged: the camera takes no new drags
		bool m_TerrainToolOwnsMouse = false; // painting a terrain: no click-to-select, no gizmo
		// Click-to-select: a left press in the image, then a release without moving.
		bool m_ClickPending = false;
		ImVec2 m_ClickStart{ 0.0f, 0.0f };
	};
}
