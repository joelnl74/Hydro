#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Hydro
{
	class Texture2D;

	// One character of a bitmap font, in the font's pixels (BMFont's "char" line).
	struct FontGlyph
	{
		glm::vec2 SourcePosition{ 0.0f, 0.0f }; // top-left in its page, Y down
		glm::vec2 SourceSize{ 0.0f, 0.0f };     // 0 for a glyph without ink (space)
		glm::vec2 Offset{ 0.0f, 0.0f };         // from the pen (at the line's top) to the glyph's top-left, Y down
		float Advance = 0.0f;                   // how far the pen moves after it
		uint32_t Page = 0;
	};

	// Everything a BMFont file says, without its textures: what TextLayout works from.
	struct FontData
	{
		std::string Face;
		float Size = 0.0f;       // the size it was made at, in pixels
		float LineHeight = 0.0f; // from one line's top to the next's
		float Base = 0.0f;       // from a line's top to its baseline
		glm::vec2 PageSize{ 1.0f, 1.0f };
		std::vector<std::string> PageFiles; // relative to the .fnt file
		std::unordered_map<uint32_t, FontGlyph> Glyphs;   // by Unicode code point
		std::unordered_map<uint64_t, float> Kerning;      // (first << 32) | second -> extra advance

		const FontGlyph* FindGlyph(uint32_t codepoint) const
		{
			const auto found = Glyphs.find(codepoint);
			return found != Glyphs.end() ? &found->second : nullptr;
		}

		float GetKerning(uint32_t first, uint32_t second) const
		{
			const auto found = Kerning.find(((uint64_t)first << 32) | (uint64_t)second);
			return found != Kerning.end() ? found->second : 0.0f;
		}
	};

	// A bitmap font asset (runtime UI, UI2): an AngelCode BMFont in its text format (.fnt, written by BMFont, Hiero,
	// fontbm or scripts/fonts/make_bmfont.py) and its page images, loaded as Texture2D assets. Pixel fonts want their
	// pages imported with nearest filtering (the page's .hymeta); a page that isn't is warned about once.
	// The AssetManager loads it (AssetManager::GetAsset<Font>); text is laid out by TextLayout and drawn as quads.
	class Font : public Asset
	{
	public:
		Font(const FontData& data, std::vector<Ref<Texture2D>> pages)
			: m_Data(data), m_Pages(std::move(pages)) {}

		Font(const Font&) = delete;
		Font& operator=(const Font&) = delete;

		// Reads a .fnt file and loads its pages (from the same folder), without sharing: every call makes a new
		// asset. nullptr (logged) when the file can't be read or a page can't be loaded. The AssetManager's loader calls this.
		static Ref<Font> LoadUncached(const std::filesystem::path& path);
		// Parses the text format of a BMFont file. False (with `error` set) when it isn't one: no "common" line,
		// no pages or no characters. Unknown lines and keys are ignored.
		static bool Parse(const std::string& text, FontData& data, std::string& error);

		static AssetType GetStaticType() { return AssetType::Font; }
		AssetType GetType() const override { return GetStaticType(); }

		const FontData& GetData() const { return m_Data; }
		uint32_t GetPageCount() const { return (uint32_t)m_Pages.size(); }
		// The page's texture; null for an index out of range.
		const Ref<Texture2D>& GetPage(uint32_t index) const;

		// True the first time it is called for `codepoint`, so a missing character is logged once per font.
		bool ShouldReportMissing(uint32_t codepoint) const;

	private:
		FontData m_Data;
		std::vector<Ref<Texture2D>> m_Pages;
		mutable std::unordered_set<uint32_t> m_ReportedMissing;
	};
}
