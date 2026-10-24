#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>

namespace Hydro
{
	// What the editor draws for an image asset in a tile or a list row.
	struct Thumbnail
	{
		// The small image (at most ThumbnailBaker::MaxSize on its longest side); null while it is being
		// prepared, and when the file can't be read.
		Ref<Texture2D> Texture;
		// Still to come: draw a placeholder this frame.
		bool Pending = false;
		// The image is a sprite sheet.
		bool IsSheet = false;
	};

	// Small copies of the project's images for the editor (Project window tiles, asset picker rows), so
	// browsing a folder of big images doesn't load them whole. Keyed by asset handle, so a renamed or
	// moved image keeps its thumbnail.
	//
	// A thumbnail is made once from the decoded image (ThumbnailBaker) and saved in
	// "<project>/.hydro/thumbnails/<handle>.thumb"; the editor reads that file next time instead of decoding
	// the image again. The file knows the image's size, date and import settings it was made from, so a
	// changed image or .hymeta bakes a new one (checked about once a second while a tile is drawn). A few
	// are made per frame, the rest are Pending, so a folder of hundreds of images fills in instead of freezing.
	// Nothing here goes through the AssetManager's cache: the thumbnails are the editor's, and are let go
	// by Clear() when another project opens.
	class ThumbnailManager
	{
	public:
		// The thumbnail of an image asset. Call it while drawing, every frame it is shown: it is cheap when
		// nothing changed. Handles that aren't images, or whose file is missing, give an empty Thumbnail.
		static const Thumbnail& Get(AssetHandle handle);

		// Lets go of every thumbnail (another project opened). The files stay.
		static void Clear();
	};
}
