#include <hypch.h>
#include "EditorCamera.h"

#include "Hydro/Core/Input.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	namespace
	{
		constexpr float RotationSpeed = 0.005f;   // radians per pixel
		constexpr float DragZoomSpeed = 0.005f;   // per pixel, exponential
		constexpr float ScrollZoomFactor = 0.85f; // distance multiplier per wheel step
		constexpr float MinDistance = 0.01f;
		constexpr float MaxDistance = 100000.0f;
		constexpr float MaxPitch = 1.55f;         // just under 90 degrees, so the view never flips

		bool IsAltPressed()
		{
			return Input::IsKeyPressed(KeyCode::LeftAlt) || Input::IsKeyPressed(KeyCode::RightAlt);
		}
	}

	EditorCamera::EditorCamera()
	{
		Reset();
	}

	void EditorCamera::Reset()
	{
		m_FocalPoint = glm::vec3(0.0f);
		m_Pitch = 0.0f;
		m_Yaw = 0.0f;

		// 10 units visible vertically, the same as a default SceneCamera.
		m_Distance = 5.0f / std::tan(glm::radians(m_VerticalFOV) * 0.5f);

		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::Focus(const glm::vec3& point)
	{
		m_FocalPoint = point;
		UpdateView();
	}

	void EditorCamera::SetViewportSize(float width, float height)
	{
		if (width < 1.0f || height < 1.0f)
			return;

		if (width == m_ViewportWidth && height == m_ViewportHeight)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
		UpdateProjection();
	}

	void EditorCamera::SetProjectionType(ProjectionType type)
	{
		if (type == m_ProjectionType)
			return;

		m_ProjectionType = type;

		// The orthographic view is the 2D view: straight down -Z.
		if (m_ProjectionType == ProjectionType::Orthographic)
		{
			m_Pitch = 0.0f;
			m_Yaw = 0.0f;
		}

		UpdateView();
		UpdateProjection();
	}

	void EditorCamera::Update(bool acceptInput)
	{
		const auto [mouseX, mouseY] = Input::GetMousePosition();
		const glm::vec2 mouse{ mouseX, mouseY };
		const glm::vec2 delta = mouse - m_LastMousePosition;
		m_LastMousePosition = mouse;

		// A drag ends when the button that started it is released, wherever the mouse is.
		if (m_DragMode != DragMode::None && Input::IsMouseButtonPressed((MouseButton)m_DragButton) == false)
			m_DragMode = DragMode::None;

		// A new drag only starts over the viewport.
		if (m_DragMode == DragMode::None && acceptInput)
		{
			const bool alt = IsAltPressed();

			if (Input::IsMouseButtonPressed(MouseButton::Middle))
			{
				m_DragMode = DragMode::Pan;
				m_DragButton = (int)MouseButton::Middle;
			}
			else if (alt && Input::IsMouseButtonPressed(MouseButton::Left))
			{
				m_DragMode = m_ProjectionType == ProjectionType::Perspective ? DragMode::Orbit : DragMode::Pan;
				m_DragButton = (int)MouseButton::Left;
			}
			else if (alt && Input::IsMouseButtonPressed(MouseButton::Right))
			{
				m_DragMode = DragMode::Zoom;
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
			m_FocalPoint += (-GetRight() * delta.x + GetUp() * delta.y) * unitsPerPixel;
			break;
		}
		case DragMode::Orbit:
		{
			m_Yaw += delta.x * RotationSpeed;
			m_Pitch = std::clamp(m_Pitch + delta.y * RotationSpeed, -MaxPitch, MaxPitch);
			break;
		}
		case DragMode::Zoom:
		{
			// Drag up or right to zoom in.
			m_Distance = std::clamp(m_Distance * std::exp((delta.y - delta.x) * DragZoomSpeed), MinDistance, MaxDistance);
			UpdateProjection();
			break;
		}
		case DragMode::None:
			return;
		}

		UpdateView();
	}

	void EditorCamera::Zoom(float scrollDelta)
	{
		m_Distance = std::clamp(m_Distance * std::pow(ScrollZoomFactor, scrollDelta), MinDistance, MaxDistance);
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
			m_Projection = Camera::Perspective(glm::radians(m_VerticalFOV), aspectRatio, m_NearClip, m_FarClip);
		else
			m_Projection = Camera::Orthographic(GetViewHeight(), aspectRatio, -m_FarClip, m_FarClip);
	}

	glm::quat EditorCamera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
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
		return 2.0f * m_Distance * std::tan(glm::radians(m_VerticalFOV) * 0.5f);
	}

	float EditorCamera::GetWorldUnitsPerPixel() const
	{
		return GetViewHeight() / m_ViewportHeight;
	}
}
