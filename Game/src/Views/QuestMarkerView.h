// Game/src/Views/QuestMarkerView.h
#pragma once

#include "Hydro.h"

#include <cstdint>

namespace Hydro
{
	// The look of the marker that floats over a quest giver (Nov 3, 2026): black on yellow, as quest markers are
	// everywhere: "!" while the giver has a quest to offer, "?" when one can be turned in. The two images carry their
	// own colours, so the marker's image is drawn untinted. Used by QuestGiverScript and ShrineQuestScript; the moving
	// and the showing stay with them.
	class QuestMarkerView
	{
	public:
		static constexpr uint64_t DefaultOfferMark = 2768530010260909562ull;  // Assets/Sprites/UI/QuestMark_Offer.png
		static constexpr uint64_t DefaultReturnMark = 4835287365312174536ull; // Assets/Sprites/UI/QuestMark_Return.png

		// Shows the offer or the turn-in mark on the marker's image (an entity with a UIImageComponent).
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
