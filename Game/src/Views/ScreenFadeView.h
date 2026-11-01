// Game/src/Views/ScreenFadeView.h
#pragma once

#include "Views/ViewHelpers.h"

namespace Hydro
{
	// The black over the whole view ("Fade" on the field's topmost canvas, scripts/ui/build_game_ui.py): 0 clear,
	// 1 black; hidden while clear. A view (Scripting plan SC8f): it shows what it is told, the director decides when.
	class ScreenFadeView
	{
	public:
		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			m_Fade = ViewHelpers::Find(scene, "Fade", "Screen fade");
		}

		void Set(float alpha)
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			Entity fade{ m_Fade, m_Scene };
			ViewHelpers::SetAlpha(fade, alpha);
			ViewHelpers::SetVisible(fade, alpha > 0.0f);
		}

	private:
		Scene* m_Scene = nullptr;
		entt::entity m_Fade = entt::null;
	};
}
