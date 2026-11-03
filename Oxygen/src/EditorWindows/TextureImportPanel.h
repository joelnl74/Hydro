#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Renderer/TextureImport.h>

#include <filesystem>
#include <functional>

namespace Hydro
{
	// The inspector's view of an image file: a preview and its import settings. Apply writes "<image>.hymeta"
	// and asks the editor to re-import it.
	class TextureImportPanel
	{
	public:
		std::function<void(const std::filesystem::path&)> OnReimported;     // the .hymeta was saved
		std::function<void(const std::filesystem::path&)> OnOpenSpriteEditor;

		void OnImguiRender(const std::filesystem::path& imagePath);

	private:
		// Reads the file's current settings and drops unapplied edits.
		void Load(const std::filesystem::path& imagePath);
		void Apply();

	private:
		std::filesystem::path m_Path;
		std::filesystem::file_time_type m_LoadedImportTime{};
		Ref<Texture2D> m_Texture;
		TextureImportConfig m_Config; // being edited
		bool m_Dirty = false;
	};
}
