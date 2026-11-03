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

	// The editor's field for an asset reference: a searchable list, drag and drop from the Project window and a clear
	// button. It only reports the choice: the caller checks that the asset loads, then stores the handle.
	class AssetPicker
	{
	public:
		// `id` must be unique where it is drawn. `detail` goes in the tooltip under the path (an image's size).
		// `width` 0 fills the rest of the line.
		static AssetPickerResult DrawField(const char* id, AssetHandle current, AssetType type, const std::string& detail = {},
			float width = 0.0f);
		// The same for game data: a definition or table (`type`) holding `dataType`, an HY_DATA type name.
		static AssetPickerResult DrawDataField(const char* id, AssetHandle current, AssetType type, const std::string& dataType,
			float width = 0.0f);
	private:
		static AssetPickerResult DrawFiltered(const char* id, AssetHandle current, AssetType type, const std::string& dataType,
			const std::string& detail, float width);
	};
}
