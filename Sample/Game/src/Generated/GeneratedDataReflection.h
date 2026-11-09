#pragma once

namespace Hydro
{
	// Generated registrations for every HY_DATA struct and HY_ENUM enum found by
	// Tools/ReflectionGenerator/generate_reflection.py (into DataTypeRegistry).
	// Safe to call more than once; registration runs once per process.
	void RegisterGeneratedDataReflection();
}
