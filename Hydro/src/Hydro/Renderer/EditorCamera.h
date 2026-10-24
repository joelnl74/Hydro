#pragma once
#include "Hydro/Renderer/Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hydro
{
	// Live options of an EditorCamera. The editor keeps them in its own settings and passes them in
	// with EditorCamera::SetSettings; they apply immediately.
	struct EditorCameraSettings
	{
		float VerticalFOV = 45.0f; // degrees (perspective)
		float NearClip = 0.01f;
		float FarClip = 1000.0f;

		// Multipliers on the default mouse speeds.
		float OrbitSpeed = 1.0f;
		float PanSpeed = 1.0f;   // 1: the scene follows the cursor exactly
		float ZoomSpeed = 1.0f;
	};

	// Free camera for looking at a scene in an editor viewport. Not part of the scene: the editor
	// owns one and passes it to SceneRenderer::RenderEditor.
	//
	// It orbits a focal point. Controls (mouse, read through Input while Update is called):
	//  - Alt + left drag:   orbit (perspective only)
	//  - Middle drag:       pan (also Alt + left drag in orthographic mode)
	//  - Alt + right drag:  zoom
	//  - Scroll:            zoom (call Zoom with the wheel delta)
	// Orthographic mode is the 2D view: it looks straight down -Z and can't orbit.
	class EditorCamera : public Camera
	{
	public:
		EditorCamera();

		// Applies mouse drags since the last call. A drag only starts while acceptInput is true (the
		// viewport is hovered), but keeps going until its button is released. Call it every frame.
		void Update(bool acceptInput);
		// Zoom towards the focal point by a mouse-wheel delta (positive = closer).
		void Zoom(float scrollDelta);

		void SetViewportSize(float width, float height);
		// Keeps what is framed at the focal point when the field of view changes.
		void SetSettings(const EditorCameraSettings& settings);
		const EditorCameraSettings& GetSettings() const { return m_Settings; }
		void SetProjectionType(ProjectionType type);
		ProjectionType GetProjectionType() const { return m_ProjectionType; }

		// Back to the default view: looking down -Z at the origin, 10 units visible vertically.
		void Reset();
		void Focus(const glm::vec3& point);
		// Centres on point and zooms so a square of frameSize world units fills about a third of the
		// view height (never closer than minHeight units of view), e.g. to look at a selected entity.
		void Frame(const glm::vec3& point, float frameSize, float minHeight = 6.0f);

		const glm::mat4& GetView() const { return m_View; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_View; }

		glm::vec3 GetPosition() const;
		const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }
		float GetDistance() const { return m_Distance; }

		glm::vec3 GetForward() const;
		glm::vec3 GetRight() const;
		glm::vec3 GetUp() const;

	private:
		enum class DragMode { None, Orbit, Pan, Zoom };

		void UpdateView();
		void UpdateProjection();
		glm::quat GetOrientation() const;
		// Height of the view at the focal point; also the orthographic size, so switching between
		// perspective and orthographic keeps the focal plane framed the same.
		float GetViewHeight() const;
		float GetWorldUnitsPerPixel() const;

	private:
		ProjectionType m_ProjectionType = ProjectionType::Orthographic;
		EditorCameraSettings m_Settings;

		glm::mat4 m_View{ 1.0f };
		glm::vec3 m_FocalPoint{ 0.0f };
		float m_Distance = 10.0f;
		float m_Pitch = 0.0f; // radians
		float m_Yaw = 0.0f;   // radians

		float m_ViewportWidth = 1280.0f;
		float m_ViewportHeight = 720.0f;

		DragMode m_DragMode = DragMode::None;
		int m_DragButton = 0;
		glm::vec2 m_LastMousePosition{ 0.0f };
	};
}
