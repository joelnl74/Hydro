#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>

namespace Hydro
{
	// What the editor draws for an image asset in a tile or a list row.
	struct Thumbnail
	{
		// At most ThumbnailBaker::MaxSize on its longest side; null while pending and when the file can't be read.
		Ref<Texture2D> Texture;
		// Still to come: draw a placeholder this frame.
		bool Pending = false;
		// The image is a sprite sheet.
		bool IsSheet = false;
	};

	// Small copies of the project's images for the editor, keyed by handle and baked to .hydro/thumbnails so they
	// aren't decoded again. A changed image rebakes; a few are made per frame, so big folders fill in instead of freezing.
	class ThumbnailManager
	{
	public:
		// Call it every frame the thumbnail is shown; cheap when nothing changed. Non-images give an empty Thumbnail.
		static const Thumbnail& Get(AssetHandle handle);

		// Lets go of every thumbnail (another project opened). The files stay.
		static void Clear();
	};
}
