#pragma once

#include "Hydro.h"

#include <cstdint>

namespace Hydro
{
	// The marker over a quest giver: "!" with a quest to offer, "?" when one can be turned in. The images carry their own
	// colours, so the marker is drawn untinted.
	class QuestMarkerView
	{
	public:
		static constexpr uint64_t DefaultOfferMark = 2768530010260909562ull;  // Assets/Sprites/UI/QuestMark_Offer.png
		static constexpr uint64_t DefaultReturnMark = 4835287365312174536ull; // Assets/Sprites/UI/QuestMark_Return.png

		// `image` is an entity with a UIImageComponent.
		static void Present(Entity image, bool turnIn, AssetHandle offerMark, AssetHandle returnMark)
		{
			if (image.IsValid() == false || image.HasComponent<UIImageComponent>() == false)
			{
				return;
			}
			UIImageComponent& ui = image.GetComponent<UIImageComponent>();
			const AssetHandle wanted = turnIn ? returnMark : offerMark;
			if ((uint64_t)wanted != 0 && (uint64_t)ui.Texture != (uint64_t)wanted)
			{
				AssetManager::GetAsset<Texture2D>(wanted); // loaded before it is assigned (the renderer never loads)
				ui.Texture = wanted;
			}
			ui.Color = glm::vec4(1.0f);
		}
	};
}
