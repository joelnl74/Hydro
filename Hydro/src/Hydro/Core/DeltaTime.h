#pragma once

namespace Hydro
{
	class DeltaTime
	{
	public:
		DeltaTime(float time) : m_deltaTime(time) {};
		const float const GetTime() { return m_deltaTime; };
	private:
		float m_deltaTime;
	};
}
