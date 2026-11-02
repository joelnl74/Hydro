#include "hypch.h"
#include "SpriteBounds.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Hydro
{
	glm::mat4 SpriteBounds::GetQuad(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex)
	{
		if (texture == nullptr || texture->GetSpriteMode() != SpriteMode::Multiple)
		{
			return transform;
		}

		// The quad is centred on the entity; move it so the sprite's pivot is there instead
		// (a feet pivot keeps a character standing on its position while frames change).
		const glm::vec2 pivot = texture->GetSprite(spriteIndex).Pivot;
		if (pivot == glm::vec2(0.5f))
		{
			return transform;
		}
		return transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.5f - pivot.x, 0.5f - pivot.y, 0.0f));
	}

	bool SpriteBounds::GetStanding(Entity entity, glm::vec3& foot, float& width, float& height)
	{
		const SpriteRendererComponent* sprite = entity.TryGetComponent<SpriteRendererComponent>();
		const WorldTransformComponent* transform = entity.TryGetComponent<WorldTransformComponent>();
		if (sprite == nullptr || transform == nullptr || sprite->Overlay || sprite->Facing == SpriteFacing::Flat)
		{
			return false;
		}
		const Ref<Texture2D> texture = (uint64_t)sprite->Texture != 0 ? AssetManager::GetLoadedAsset<Texture2D>(sprite->Texture) : nullptr;
		const glm::mat4 quad = GetQuad(transform->Matrix, texture.get(), sprite->SpriteIndex);
		width = glm::length(glm::vec3(quad[0]));
		height = glm::length(glm::vec3(quad[1]));
		if (sprite->Facing == SpriteFacing::Upright)
		{
			foot = glm::vec3(quad * glm::vec4(0.0f, -0.5f, 0.0f, 1.0f));
			return true;
		}
		foot = glm::vec3(quad * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) - glm::vec3(0.0f, 0.0f, 0.5f * height);
		return true;
	}
}
