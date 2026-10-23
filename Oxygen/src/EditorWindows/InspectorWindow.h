#pragma once
#include "Hydro/Scene/Entity.h"
#include "TextureImportPanel.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <filesystem>
#include <functional>

namespace Hydro
{
	// Shows and edits the components of the entity selected in the hierarchy, or, when no entity is
	// selected, the asset selected in the Project window (an image's import settings, a .hyanim's clips).
	// Editor UI only: it reads and writes component data, it holds no behaviour of its own.
	class InspectorWindow
	{
	public:
		// Set by the editor layer.
		std::function<void(const std::filesystem::path&)> OnTextureReimported;
		std::function<void(const std::filesystem::path&)> OnOpenSpriteEditor;
		std::function<void(const std::filesystem::path&)> OnOpenAnimation;

		InspectorWindow() = default;
		~InspectorWindow() = default;

		// `asset` is shown when `entity` isn't valid (empty: nothing).
		void OnImguiRender(Entity entity, const std::filesystem::path& asset);

	private:
		void DrawEntityHeader(Entity entity);
		void DrawComponents(Entity entity);
		void DrawAddComponentButton(Entity entity);
		// One menu item per registered script; picking one binds it to the entity (adding its
		// NativeScriptComponent if needed).
		void DrawScriptMenuItems(Entity entity);
		void DrawPrefabButton(Entity entity);
		void DrawAsset(const std::filesystem::path& asset);
		void DrawAnimationSetAsset(const std::filesystem::path& path);

	private:
		TextureImportPanel m_TextureImportPanel;
		// Held so the shared set isn't loaded again every frame (LoadFromFile shares only live sets).
		Ref<SpriteAnimationSet> m_InspectedAnimationSet;
		std::filesystem::path m_InspectedAnimationPath;
		std::filesystem::file_time_type m_InspectedAnimationTime{};
	};
}
