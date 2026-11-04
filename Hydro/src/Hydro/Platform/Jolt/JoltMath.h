#pragma once

#include <Jolt/Jolt.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// glm values to Jolt's and back.

namespace Hydro
{
	class JoltMath
	{
	public:
		static JPH::Vec3 ToJolt(const glm::vec3& value) { return JPH::Vec3(value.x, value.y, value.z); }
		static JPH::RVec3 ToJoltPosition(const glm::vec3& value) { return JPH::RVec3(value.x, value.y, value.z); }
		static glm::vec3 ToGlm(JPH::Vec3Arg value) { return { value.GetX(), value.GetY(), value.GetZ() }; }

		static glm::vec3 ToGlmPosition(JPH::RVec3Arg value)
		{
			return { (float)value.GetX(), (float)value.GetY(), (float)value.GetZ() };
		}

		static JPH::Quat ToJoltRotation(const glm::quat& rotation)
		{
			return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w).Normalized();
		}

		// Jolt's capsules and cylinders stand along Y; Hydro's along Z.
		static JPH::Quat StandUp() { return JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI); }
	};
}
