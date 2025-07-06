#pragma once

namespace Hydro
{
	class UUID
	{
	public:
		UUID();
		UUID(long uuid);
		UUID(const UUID&) = default;

		operator uint64_t () { return m_UUID; }
		operator const uint64_t() const { return m_UUID; }
	private:
		uint64_t m_UUID;
	};
}
