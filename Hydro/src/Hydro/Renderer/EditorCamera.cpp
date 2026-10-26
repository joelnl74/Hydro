#include <hypch.h>
#include "EditorCamera.h"

#include "Hydro/Core/Input.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	constexpr float RotationSpeed = 0.005f;   // radians per pixel (orbit and fly look)
	constexpr float DragZoomSpeed = 0.005f;   // per pixel, exponential
	constexpr float ScrollZoomFactor = 0.85f; // distance multiplier per wheel step
	constexpr float FlyScrollFactor = 1.25f;  // fly speed multiplier per wheel step while flying
	constexpr float FlyFastMultiplier = 3.0f; // with Shift
	constexpr float MinFlySpeedScale = 0.05f;
	constexpr float MaxFlySpeedScale = 50.0f;
	constexpr float MaxFlyStep = 0.1f;        // seconds: a hitch doesn't throw the camera away
	constexpr float MinDistance = 0.01f;
	constexpr float MaxDistance = 100000.0f;
	// From straight down (0) past level (90 degrees) to 175: looking up from under the ground is allowed,
	// straight up isn't (the view would flip).
	constexpr float MaxPitch = 3.0543262f;    // 175 degrees
	constexpr float DioramaPitchDegrees = 50.0f;

	static bool IsAltPressed()
	{
		return Input::IsKeyPressed(KeyCode::LeftAlt) || Input::IsKeyPressed(KeyCode::RightAlt);
	}

	static bool IsShiftPressed()
	{
		return Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::RightShift);
	}

	static float KeyAxis(KeyCode negative, KeyCode positive)
	{
		return (Input::IsKeyPressed(positive) ? 1.0f : 0.0f) - (Input::IsKeyPressed(negative) ? 1.0f : 0.0f);
	}

	EditorCamera::EditorCamera()
	{
		Reset();
	}

	void EditorCamera::Reset()
	{
		m_FocalPoint = glm::vec3(0.0f);
		m_PerspectivePitch = glm::radians(DioramaPitchDegrees);
		m_PerspectiveYaw = 0.0f;
		if (m_ProjectionType == ProjectionType::Perspective)
		{
			m_Pitch = m_PerspectivePitch;
			m_Yaw = m_PerspectiveYaw;
		}
		else
		{
			m_Pitch = 0.0f;
			m_Yaw = 0.0f;
		}

		// 10 units visible vertically, the same as a default SceneCamera.
		m_Distance = 5.0f / std::tan(glm::radians(m_Settings.VerticalFOV) * 0.5f);

		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::Focus(const glm::vec3& point)
	{
		m_FocalPoint = point;
		UpdateView();
	}

	void EditorCamera::Frame(const glm::vec3& point, float frameSize, float minHeight)
	{
		const float viewHeight = std::max(frameSize * 3.0f, minHeight);
		m_FocalPoint = point;
		m_Distance = std::clamp(viewHeight * 0.5f / std::tan(glm::radians(m_Settings.VerticalFOV) * 0.5f), MinDistance, MaxDistance);
		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::SetViewAngles(float pitchDegrees, float yawDegrees)
	{
		if (m_ProjectionType == ProjectionType::Orthographic)
		{
			return;
		}

		m_Pitch = std::clamp(glm::radians(pitchDegrees), 0.0f, MaxPitch);
		m_Yaw = glm::radians(yawDegrees);
		UpdateView();
	}

	void EditorCamera::SetView(EditorCameraView view)
	{
		SetProjectionType(ProjectionType::Perspective);

		// Yaw turns the view about +Z: 0 looks north (+Y), -90 east (+X), 90 west, 180 south.
		switch (view)
		{
			case EditorCameraView::Top: SetViewAngles(0.0f, 0.0f); break;
			case EditorCameraView::Diorama: SetViewAngles(DioramaPitchDegrees, 0.0f); break;
			case EditorCameraView::Front: SetViewAngles(90.0f, 0.0f); break;
			case EditorCameraView::Back: SetViewAngles(90.0f, 180.0f); break;
			case EditorCameraView::Left: SetViewAngles(90.0f, -90.0f); break;
			case EditorCameraView::Right: SetViewAngles(90.0f, 90.0f); break;
		}
	}

	float EditorCamera::GetPitchDegrees() const
	{
		return glm::degrees(m_Pitch);
	}

	float EditorCamera::GetYawDegrees() const
	{
		return glm::degrees(m_Yaw);
	}

	float EditorCamera::GetFlySpeed() const
	{
		return m_Settings.FlySpeed * m_FlySpeedScale;
	}

	void EditorCamera::SetViewportSize(float width, float height)
	{
		if (width < 1.0f || height < 1.0f)
		{
			return;
		}

		if (width == m_ViewportWidth && height == m_ViewportHeight)
		{
			return;
		}

		m_ViewportWidth = width;
		m_ViewportHeight = height;
		UpdateProjection();
	}

	void EditorCamera::SetSettings(const EditorCameraSettings& settings)
	{
		const EditorCameraSettings& current = m_Settings;
		const bool unchanged = settings.VerticalFOV == current.VerticalFOV && settings.NearClip == current.NearClip &&
			settings.FarClip == current.FarClip && settings.OrbitSpeed == current.OrbitSpeed &&
			settings.PanSpeed == current.PanSpeed && settings.ZoomSpeed == current.ZoomSpeed &&
			settings.FlySpeed == current.FlySpeed;
		if (unchanged)
		{
			return;
		}

		EditorCameraSettings clamped = settings;
		clamped.VerticalFOV = std::clamp(clamped.VerticalFOV, 5.0f, 150.0f);
		clamped.NearClip = std::max(clamped.NearClip, 0.0001f);
		clamped.FarClip = std::max(clamped.FarClip, clamped.NearClip * 2.0f);
		clamped.FlySpeed = std::max(clamped.FlySpeed, 0.01f);

		// Same view height at the focal point with the new field of view, so changing it doesn't zoom.
		const float viewHeight = GetViewHeight();
		m_Settings = clamped;
		m_Distance = std::clamp(viewHeight * 0.5f / std::tan(glm::radians(m_Settings.VerticalFOV) * 0.5f), MinDistance, MaxDistance);

		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::SetProjectionType(ProjectionType type)
	{
		if (type == m_ProjectionType)
		{
			return;
		}

		m_ProjectionType = type;
		m_DragMode = DragMode::None;

		if (m_ProjectionType == ProjectionType::Orthographic)
		{
			// The orthographic view is the 2D view: straight down -Z. Remember the 3D angle for the way back.
			m_PerspectivePitch = m_Pitch;
			m_PerspectiveYaw = m_Yaw;
			m_Pitch = 0.0f;
			m_Yaw = 0.0f;
		}
		else
		{
			m_Pitch = m_PerspectivePitch;
			m_Yaw = m_PerspectiveYaw;
		}

		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::Update(float deltaTime, bool acceptInput)
	{
		const auto [mouseX, mouseY] = Input::GetMousePosition();
		const glm::vec2 mouse{ mouseX, mouseY };
		const glm::vec2 delta = mouse - m_LastMousePosition;
		m_LastMousePosition = mouse;

		// A drag ends when the button that started it is released, wherever the mouse is.
		if (m_DragMode != DragMode::None && Input::IsMouseButtonPressed((MouseButton)m_DragButton) == false)
		{
			m_DragMode = DragMode::None;
		}

		// A new drag only starts over the viewport.
		if (m_DragMode == DragMode::None && acceptInput)
		{
			const bool alt = IsAltPressed();
			const bool perspective = m_ProjectionType == ProjectionType::Perspective;

			if (Input::IsMouseButtonPressed(MouseButton::Middle))
			{
				m_DragMode = DragMode::Pan;
				m_DragButton = (int)MouseButton::Middle;
			}
			else if (alt && Input::IsMouseButtonPressed(MouseButton::Left))
			{
				m_DragMode = perspective ? DragMode::Orbit : DragMode::Pan;
				m_DragButton = (int)MouseButton::Left;
			}
			else if (alt && Input::IsMouseButtonPressed(MouseButton::Right))
			{
				m_DragMode = DragMode::Zoom;
				m_DragButton = (int)MouseButton::Right;
			}
			else if (perspective && Input::IsMouseButtonPressed(MouseButton::Right))
			{
				m_DragMode = DragMode::Fly;
				m_DragButton = (int)MouseButton::Right;
			}

			// The first frame of a drag has no movement to apply yet.
			return;
		}

		switch (m_DragMode)
		{
			case DragMode::Pan:
			{
				// The scene follows the cursor: screen Y points down, world Y up.
				const float unitsPerPixel = GetWorldUnitsPerPixel();
				m_FocalPoint += (-GetRight() * delta.x + GetUp() * delta.y) * unitsPerPixel * m_Settings.PanSpeed;
				break;
			}
			case DragMode::Orbit:
			{
				// Drag right: the camera goes round to the left (the scene turns right); drag down: the
				// camera rises towards the top-down view.
				const float speed = RotationSpeed * m_Settings.OrbitSpeed;
				m_Yaw -= delta.x * speed;
				m_Pitch = std::clamp(m_Pitch - delta.y * speed, 0.0f, MaxPitch);
				break;
			}
			case DragMode::Zoom:
			{
				// Drag up or right to zoom in.
				m_Distance = std::clamp(m_Distance * std::exp((delta.y - delta.x) * DragZoomSpeed * m_Settings.ZoomSpeed), MinDistance, MaxDistance);
				UpdateProjection();
				break;
			}
			case DragMode::Fly:
			{
				ApplyFly(deltaTime, delta);
				break;
			}
			case DragMode::None:
			{
				return;
			}
		}

		UpdateView();
	}

	void EditorCamera::ApplyFly(float deltaTime, const glm::vec2& mouseDelta)
	{
		// Look around from where the camera is: it turns, the focal point moves with its view.
		const glm::vec3 position = GetPosition();
		const float speed = RotationSpeed * m_Settings.OrbitSpeed;
		m_Yaw -= mouseDelta.x * speed;
		m_Pitch = std::clamp(m_Pitch - mouseDelta.y * speed, 0.0f, MaxPitch);
		m_FocalPoint = position + GetForward() * m_Distance;

		// W/S along the view, A/D sideways, Q/E down/up the world's Z.
		const glm::vec3 move = GetForward() * KeyAxis(KeyCode::S, KeyCode::W) + GetRight() * KeyAxis(KeyCode::A, KeyCode::D) +
			glm::vec3(0.0f, 0.0f, 1.0f) * KeyAxis(KeyCode::Q, KeyCode::E);
		if (glm::dot(move, move) > 0.0f)
		{
			const float metresPerSecond = GetFlySpeed() * (IsShiftPressed() ? FlyFastMultiplier : 1.0f);
			m_FocalPoint += glm::normalize(move) * metresPerSecond * std::clamp(deltaTime, 0.0f, MaxFlyStep);
		}
	}

	void EditorCamera::Zoom(float scrollDelta)
	{
		if (m_DragMode == DragMode::Fly)
		{
			m_FlySpeedScale = std::clamp(m_FlySpeedScale * std::pow(FlyScrollFactor, scrollDelta), MinFlySpeedScale, MaxFlySpeedScale);
			return;
		}

		m_Distance = std::clamp(m_Distance * std::pow(ScrollZoomFactor, scrollDelta * m_Settings.ZoomSpeed), MinDistance, MaxDistance);
		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::UpdateView()
	{
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), GetPosition()) * glm::mat4_cast(GetOrientation());
		m_View = glm::inverse(transform);
	}

	void EditorCamera::UpdateProjection()
	{
		const float aspectRatio = m_ViewportWidth / m_ViewportHeight;

		if (m_ProjectionType == ProjectionType::Perspective)
		{
			m_Projection = Camera::Perspective(glm::radians(m_Settings.VerticalFOV), aspectRatio, m_Settings.NearClip, m_Settings.FarClip);
		}
		else
		{
			m_Projection = Camera::Orthographic(GetViewHeight(), aspectRatio, -m_Settings.FarClip, m_Settings.FarClip);
		}
	}

	glm::quat EditorCamera::GetOrientation() const
	{
		// Turntable: tip the top-down camera (looking down -Z, +Y up on screen) by the pitch about X, so it
		// looks towards +Y, then turn it about the world's Z by the yaw. No roll, whatever the angles.
		return glm::angleAxis(m_Yaw, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::angleAxis(m_Pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetPosition() const
	{
		return m_FocalPoint - GetForward() * m_Distance;
	}

	glm::vec3 EditorCamera::GetForward() const
	{
		return GetOrientation() * glm::vec3(0.0f, 0.0f, -1.0f);
	}

	glm::vec3 EditorCamera::GetRight() const
	{
		return GetOrientation() * glm::vec3(1.0f, 0.0f, 0.0f);
	}

	glm::vec3 EditorCamera::GetUp() const
	{
		return GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
	}

	float EditorCamera::GetViewHeight() const
	{
		return 2.0f * m_Distance * std::tan(glm::radians(m_Settings.VerticalFOV) * 0.5f);
	}

	float EditorCamera::GetWorldUnitsPerPixel() const
	{
		return GetViewHeight() / m_ViewportHeight;
	}
}
