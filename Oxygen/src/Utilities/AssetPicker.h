#pragma once
#include <Hydro/Asset/Asset.h>

#include <string>

namespace Hydro
{
	// What the user did to a picker field this frame.
	struct AssetPickerResult
	{
		bool Changed = false;
		// The asset picked or dropped; 0 when the field was cleared. Only meaningful when Changed.
		AssetHandle Handle = 0;
	};

	// The field for an asset reference in the editor (a sprite's texture, an animator's .hyanim): shows the
	// asset's file name, opens a searchable list of the project's assets of one type (with thumbnails for
	// images), accepts an asset dragged from the Project window, and has an X to clear it. It only reports
	// the choice: the caller checks that the asset loads, then stores the handle.
	class AssetPicker
	{
	public:
		// `id` must be unique among the fields drawn at the same place. `current` is the handle the field holds.
		// `detail` is shown in the tooltip under the file's path (an image's size, a set's clip count).
		// `width` is the whole field's width; 0 fills what is left of the line.
		static AssetPickerResult DrawField(const char* id, AssetHandle current, AssetType type, const std::string& detail = {}, float width = 0.0f);
	};
}
