#pragma once
#include "Hydro/Renderer/Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hydro
{
	// Live options of an EditorCamera, kept in the editor's settings and passed in with SetSettings.
	struct EditorCameraSettings
	{
		float VerticalFOV = 45.0f; // degrees (perspective)
		float NearClip = 0.01f;
		float FarClip = 1000.0f;

		// Multipliers on the default mouse speeds.
		float OrbitSpeed = 1.0f;
		float PanSpeed = 1.0f;   // 1: the scene follows the cursor exactly
		float ZoomSpeed = 1.0f;
		// Fly mode (right button held, perspective), metres per second; Shift moves faster, the wheel scales it.
		float FlySpeed = 5.0f;
	};

	// The editor viewport's views, as angles for EditorCamera::SetViewAngles (see ViewPreset).
	enum class EditorCameraView : uint8_t
	{
		Top = 0,     // straight down, north up (the 2D view's angle)
		Diorama = 1, // 50 degrees from straight down, looking north: the field scenes' tilted view
		Front = 2,   // level, looking north (+Y)
		Back = 3,    // level, looking south
		Left = 4,    // level, looking east (+X)
		Right = 5    // level, looking west
	};

	// The editor viewport's free camera: a Z-up turntable that orbits, pans, zooms and flies; orthographic is the 2D
	// view looking straight down. Controls: docs/SceneAndECS.md, "Editor camera controls".
	class EditorCamera : public Camera
	{
	public:
		EditorCamera();

		// Applies mouse drags and fly keys since the last call; call it every frame. A drag only starts while
		// acceptInput is true (the viewport is hovered), but keeps going until its button is released.
		void Update(float deltaTime, bool acceptInput);
		// Zooms towards the focal point by a wheel delta (positive = closer); while flying it changes the fly speed.
		void Zoom(float scrollDelta);

		void SetViewportSize(float width, float height);
		// Keeps what is framed at the focal point when the field of view changes.
		void SetSettings(const EditorCameraSettings& settings);
		const EditorCameraSettings& GetSettings() const { return m_Settings; }
		void SetProjectionType(ProjectionType type);
		ProjectionType GetProjectionType() const { return m_ProjectionType; }

		// Back to the default view at the origin, 10 units visible vertically: top-down in 2D, the Diorama angle in 3D.
		void Reset();
		void Focus(const glm::vec3& point);
		// Centres on point and zooms so frameSize world units fill about a third of the view height (at least minHeight).
		void Frame(const glm::vec3& point, float frameSize, float minHeight = 6.0f);

		// Degrees: pitch from straight down (0..175), yaw about +Z (0 = north). Perspective only.
		void SetViewAngles(float pitchDegrees, float yawDegrees);
		// One of the standard views (Top, Diorama, Front...); switches to perspective first.
		void SetView(EditorCameraView view);
		float GetPitchDegrees() const;
		float GetYawDegrees() const;

		// While the right button flies the camera, W/A/S/D/Q/E belong to it, not to editor shortcuts.
		bool IsFlying() const { return m_DragMode == DragMode::Fly; }
		// A mouse drag is going on: the editor keeps other mouse tools (the gizmo) out of it.
		bool IsDragging() const { return m_DragMode != DragMode::None; }
		// The fly speed this session, in metres per second (the setting times the wheel's multiplier).
		float GetFlySpeed() const;

		const glm::mat4& GetView() const { return m_View; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_View; }

		glm::vec3 GetPosition() const;
		const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }
		float GetDistance() const { return m_Distance; }

		glm::vec3 GetForward() const;
		glm::vec3 GetRight() const;
		glm::vec3 GetUp() const;

	private:
		enum class DragMode { None, Orbit, Pan, Zoom, Fly };

		void ApplyFly(float deltaTime, const glm::vec2& mouseDelta);
		void UpdateView();
		void UpdateProjection();
		glm::quat GetOrientation() const;
		// Height of the view at the focal point; also the orthographic size, so switching projection keeps the framing.
		float GetViewHeight() const;
		float GetWorldUnitsPerPixel() const;

	private:
		ProjectionType m_ProjectionType = ProjectionType::Orthographic;
		EditorCameraSettings m_Settings;

		glm::mat4 m_View{ 1.0f };
		glm::vec3 m_FocalPoint{ 0.0f };
		float m_Distance = 10.0f;
		float m_Pitch = 0.0f; // radians from straight down
		float m_Yaw = 0.0f;   // radians about +Z, 0 = looking north

		// The 3D view's angles while the 2D view is shown, to go back to them.
		float m_PerspectivePitch = 0.87266463f; // 50 degrees: the Diorama view
		float m_PerspectiveYaw = 0.0f;

		// The wheel's multiplier on Settings.FlySpeed while flying (this session only).
		float m_FlySpeedScale = 1.0f;

		float m_ViewportWidth = 1280.0f;
		float m_ViewportHeight = 720.0f;

		DragMode m_DragMode = DragMode::None;
		int m_DragButton = 0;
		glm::vec2 m_LastMousePosition{ 0.0f };
	};
}
