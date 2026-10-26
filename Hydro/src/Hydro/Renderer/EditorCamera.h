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
		// Fly mode (right mouse button held, perspective): metres per second; Shift moves faster, the
		// wheel scales it while flying.
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

	// Free camera for looking at a scene in an editor viewport. Not part of the scene: the editor
	// owns one and passes it to SceneRenderer::RenderEditor.
	//
	// The world is Z-up. The camera orbits a focal point like a turntable: yaw turns about the world's
	// Z axis, pitch is the angle from straight down (0 = top-down, 90 = level with the ground), so the
	// horizon never rolls. Controls (read through Input while Update is called):
	//  - Alt + left drag:   orbit around the focal point (perspective only)
	//  - Right drag:        fly (perspective only): look around from where the camera is, W/A/S/D move,
	//                       Q/E down/up, Shift faster, the wheel changes the speed
	//  - Middle drag:       pan (also Alt + left drag in orthographic mode)
	//  - Alt + right drag:  zoom
	//  - Scroll:            zoom (call Zoom with the wheel delta)
	// Orthographic mode is the 2D view: it looks straight down -Z and can't orbit or fly. Switching to
	// perspective goes back to the last 3D angle (the Diorama angle the first time).
	class EditorCamera : public Camera
	{
	public:
		EditorCamera();

		// Applies mouse drags and fly keys since the last call. A drag only starts while acceptInput is
		// true (the viewport is hovered), but keeps going until its button is released. Call it every frame.
		void Update(float deltaTime, bool acceptInput);
		// Zoom towards the focal point by a mouse-wheel delta (positive = closer); while flying it changes
		// the fly speed instead.
		void Zoom(float scrollDelta);

		void SetViewportSize(float width, float height);
		// Keeps what is framed at the focal point when the field of view changes.
		void SetSettings(const EditorCameraSettings& settings);
		const EditorCameraSettings& GetSettings() const { return m_Settings; }
		void SetProjectionType(ProjectionType type);
		ProjectionType GetProjectionType() const { return m_ProjectionType; }

		// Back to the default view at the origin, 10 units visible vertically: top-down in 2D, the
		// Diorama angle in 3D.
		void Reset();
		void Focus(const glm::vec3& point);
		// Centres on point and zooms so a square of frameSize world units fills about a third of the
		// view height (never closer than minHeight units of view), e.g. to look at a selected entity.
		void Frame(const glm::vec3& point, float frameSize, float minHeight = 6.0f);

		// Turns the camera around the focal point (degrees: pitch from straight down, 0..175; yaw about
		// +Z, 0 = looking north). Perspective only; ignored in the 2D view.
		void SetViewAngles(float pitchDegrees, float yawDegrees);
		// One of the standard views (Top, Diorama, Front...); switches to perspective first.
		void SetView(EditorCameraView view);
		float GetPitchDegrees() const;
		float GetYawDegrees() const;

		// True while the right mouse button flies the camera: W/A/S/D/Q/E belong to it, so the editor
		// shouldn't treat them as shortcuts.
		bool IsFlying() const { return m_DragMode == DragMode::Fly; }
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
