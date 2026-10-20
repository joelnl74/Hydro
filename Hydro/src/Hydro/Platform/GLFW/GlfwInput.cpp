#include "hypch.h"
#include "Hydro/Platform/GLFW/GlfwWindow.h"

#include "Hydro/Core/Input.h"
#include "Hydro/Core/Application.h"

namespace Hydro
{
	static GLFWwindow* GetApplicationWindow()
	{
		return static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	}

	bool Input::IsKeyPressed(KeyCode keycode)
	{
		const int state = glfwGetKey(GetApplicationWindow(), static_cast<int32_t>(keycode));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressed(MouseButton button)
	{
		return glfwGetMouseButton(GetApplicationWindow(), static_cast<int32_t>(button)) == GLFW_PRESS;
	}

	float Input::GetMouseX()
	{
		return GetMousePosition().first;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().second;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(GetApplicationWindow(), &x, &y);
		return { (float)x, (float)y };
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		glfwSetInputMode(GetApplicationWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL + (int)mode);
	}

	CursorMode Input::GetCursorMode()
	{
		return (CursorMode)(glfwGetInputMode(GetApplicationWindow(), GLFW_CURSOR) - GLFW_CURSOR_NORMAL);
	}
}
