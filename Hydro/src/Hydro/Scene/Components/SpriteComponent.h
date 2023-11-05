#pragma once
#include <glm/glm.hpp>

namespace Hydro
{
	struct SpriteComponent
	{
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		// Ref<Texture2D> Texture;
		// float TilingFactor = 1.0f;

		SpriteComponent() = default;
		SpriteComponent(const SpriteComponent& other) = default;
	};
}

