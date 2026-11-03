#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Hydro
{
	class Entity;
	class Texture2D;

	// Where a sprite entity is drawn, for game code and the renderer alike (the sprite counterpart of MeshBounds).
	class SpriteBounds
	{
	public:
		// The unit quad the sprite is drawn on before its facing turns it: centred on the entity, or offset so a sheet
		// sprite's pivot is there.
		static glm::mat4 GetQuad(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex);
		// A standing sprite as a camera-facing card: the point it stands on and its size in metres, as of the last
		// ResolveTransforms. False for Flat and Overlay sprites and entities without a sprite.
		static bool GetStanding(Entity entity, glm::vec3& foot, float& width, float& height);
	};
}
