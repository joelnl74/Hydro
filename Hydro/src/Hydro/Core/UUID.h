#pragma once

namespace Hydro
{
	class UUID
	{
	public:
		UUID();
		UUID(long uuid);
		UUID(const UUID&) = default;

		operator long() const { return m_UUID; }
	private:
		long m_UUID;
	};
}
