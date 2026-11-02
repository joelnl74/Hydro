#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Hydro
{
	class Entity;
	class Texture2D;

	// Where a sprite entity (SpriteRendererComponent) is drawn, for game code and the renderer alike: the quad before
	// it is turned for its facing, and where a standing sprite stands. The sprite counterpart of MeshBounds.
	class SpriteBounds
	{
	public:
		// The quad (a unit square scaled and placed by `transform`) the sprite is drawn on before its facing turns
		// it: centred on the entity, or moved so a Multiple texture's sprite pivot is there (SceneRenderer).
		static glm::mat4 GetQuad(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex);
		// A standing sprite as a card facing the camera: the world point it stands on (Upright: the middle of its
		// bottom edge; Billboard: below its centre) and its width and height in metres, from its
		// WorldTransformComponent (as of the last Scene::ResolveTransforms). False for Flat and Overlay sprites and
		// entities without a sprite.
		static bool GetStanding(Entity entity, glm::vec3& foot, float& width, float& height);
	};
}
