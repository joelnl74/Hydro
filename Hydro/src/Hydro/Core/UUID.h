#pragma once
#include <cstdint>
#include <functional>

namespace Hydro
{
	class UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t () { return m_UUID; }
		operator const uint64_t() const { return m_UUID; }
	private:
		uint64_t m_UUID;
	};
}

namespace std
{
	template<>
	struct hash<Hydro::UUID>
	{
		std::size_t operator()(const Hydro::UUID& uuid) const
		{
			return std::hash<uint64_t>()((uint64_t)uuid);
		}
	};
}
