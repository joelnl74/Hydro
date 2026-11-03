#pragma once
#include "Hydro/Renderer/Mesh.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Hydro
{
	// Built-in blockout shapes: Z-up, in metres, base centred on the origin, flat-shaded, counter-clockwise. UVs are in
	// metres, so a material's tiling repeats a texture per metre whatever the shape's size.
	class MeshFactory
	{
	public:
		// A flat rectangle at z = 0 facing up (size.z is ignored).
		static MeshData Plane(const glm::vec3& size);
		static MeshData Box(const glm::vec3& size);
		// A wedge rising from the front (-Y, height 0) to the back (+Y, full height).
		static MeshData Ramp(const glm::vec3& size);
		// `steps` equal steps rising towards +Y, each a solid block down to the ground.
		static MeshData Stairs(const glm::vec3& size, uint32_t steps);
		// Along Z, `segments` sides (3 to 128), with caps; size.x and size.y are the diameters.
		static MeshData Cylinder(const glm::vec3& size, uint32_t segments);
	};
}
