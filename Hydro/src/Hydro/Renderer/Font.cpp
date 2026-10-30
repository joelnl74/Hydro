#include "hypch.h"
#include "Font.h"

#include "Hydro/Renderer/Texture2D.h"

#include <fstream>
#include <iterator>
#include <sstream>

namespace Hydro
{
	// One line of a BMFont text file: its tag ("char", "common"...) and its key=value pairs (quoted values may hold
	// spaces).
	struct BMFontLine
	{
		std::string Tag;
		std::unordered_map<std::string, std::string> Values;

		int32_t GetInt(const char* key, int32_t fallback = 0) const
		{
			const auto found = Values.find(key);
			if (found == Values.end())
			{
				return fallback;
			}
			try
			{
				return (int32_t)std::stol(found->second);
			}
			catch (const std::exception&)
			{
				return fallback;
			}
		}

		std::string GetString(const char* key) const
		{
			const auto found = Values.find(key);
			return found != Values.end() ? found->second : std::string();
		}
	};

	static BMFontLine ParseBMFontLine(const std::string& text)
	{
		BMFontLine line;
		size_t position = 0;
		const size_t length = text.size();
		const auto skipSpaces = [&]()
		{
			while (position < length && (text[position] == ' ' || text[position] == '\t' || text[position] == '\r'))
			{
				position++;
			}
		};

		skipSpaces();
		const size_t tagStart = position;
		while (position < length && text[position] != ' ' && text[position] != '\t' && text[position] != '\r')
		{
			position++;
		}
		line.Tag = text.substr(tagStart, position - tagStart);

		while (position < length)
		{
			skipSpaces();
			const size_t keyStart = position;
			while (position < length && text[position] != '=' && text[position] != ' ' && text[position] != '\t' && text[position] != '\r')
			{
				position++;
			}
			const std::string key = text.substr(keyStart, position - keyStart);
			if (position >= length || text[position] != '=')
			{
				continue; // a key without a value: ignored
			}
			position++; // '='

			std::string value;
			if (position < length && text[position] == '"')
			{
				const size_t valueStart = ++position;
				while (position < length && text[position] != '"')
				{
					position++;
				}
				value = text.substr(valueStart, position - valueStart);
				if (position < length)
				{
					position++; // closing quote
				}
			}
			else
			{
				const size_t valueStart = position;
				while (position < length && text[position] != ' ' && text[position] != '\t' && text[position] != '\r')
				{
					position++;
				}
				value = text.substr(valueStart, position - valueStart);
			}
			if (key.empty() == false)
			{
				line.Values[key] = value;
			}
		}
		return line;
	}

	bool Font::Parse(const std::string& text, FontData& data, std::string& error)
	{
		data = FontData{};
		bool hasCommon = false;
		std::istringstream stream(text);
		std::string rawLine;
		while (std::getline(stream, rawLine))
		{
			const BMFontLine line = ParseBMFontLine(rawLine);
			if (line.Tag == "info")
			{
				data.Face = line.GetString("face");
				data.Size = (float)std::abs(line.GetInt("size")); // negative: matched the character height
				data.Smooth = line.GetInt("smooth") != 0;
			}
			else if (line.Tag == "common")
			{
				hasCommon = true;
				data.LineHeight = (float)line.GetInt("lineHeight");
				data.Base = (float)line.GetInt("base");
				data.PageSize = { (float)std::max(line.GetInt("scaleW", 1), 1), (float)std::max(line.GetInt("scaleH", 1), 1) };
			}
			else if (line.Tag == "page")
			{
				const int32_t id = line.GetInt("id", -1);
				if (id >= 0 && id < 256)
				{
					if ((size_t)id >= data.PageFiles.size())
					{
						data.PageFiles.resize((size_t)id + 1);
					}
					data.PageFiles[(size_t)id] = line.GetString("file");
				}
			}
			else if (line.Tag == "char")
			{
				const int32_t id = line.GetInt("id", -1);
				if (id < 0)
				{
					continue;
				}
				FontGlyph glyph;
				glyph.SourcePosition = { (float)line.GetInt("x"), (float)line.GetInt("y") };
				glyph.SourceSize = { (float)std::max(line.GetInt("width"), 0), (float)std::max(line.GetInt("height"), 0) };
				glyph.Offset = { (float)line.GetInt("xoffset"), (float)line.GetInt("yoffset") };
				glyph.Advance = (float)line.GetInt("xadvance");
				glyph.Page = (uint32_t)std::max(line.GetInt("page"), 0);
				data.Glyphs[(uint32_t)id] = glyph;
			}
			else if (line.Tag == "kerning")
			{
				const int32_t first = line.GetInt("first", -1);
				const int32_t second = line.GetInt("second", -1);
				const int32_t amount = line.GetInt("amount");
				if (first >= 0 && second >= 0 && amount != 0)
				{
					data.Kerning[((uint64_t)(uint32_t)first << 32) | (uint64_t)(uint32_t)second] = (float)amount;
				}
			}
		}

		if (hasCommon == false)
		{
			error = "no 'common' line (not a BMFont text file? Binary and XML .fnt files aren't read)";
			return false;
		}
		if (data.PageFiles.empty())
		{
			error = "no pages";
			return false;
		}
		if (data.Glyphs.empty())
		{
			error = "no characters";
			return false;
		}
		if (data.LineHeight <= 0.0f)
		{
			data.LineHeight = data.Size > 0.0f ? data.Size : 1.0f;
		}
		return true;
	}

	Ref<Font> Font::LoadUncached(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (in.is_open() == false)
		{
			HY_CORE_ERROR("Font: can't open '{}'", path.string());
			return nullptr;
		}
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		FontData data;
		std::string error;
		if (Parse(text, data, error) == false)
		{
			HY_CORE_ERROR("Font: '{}' can't be read: {}", path.string(), error);
			return nullptr;
		}

		// The pages sit next to the .fnt; in the asset folder they are Texture2D assets of their own.
		std::vector<Ref<Texture2D>> pages;
		for (const std::string& file : data.PageFiles)
		{
			const std::filesystem::path pagePath = path.parent_path() / std::filesystem::u8path(file);
			Ref<Texture2D> page = file.empty() ? nullptr : Texture2D::LoadFromFile(pagePath);
			if (page == nullptr)
			{
				HY_CORE_ERROR("Font: '{}' needs its page '{}', which can't be loaded", path.string(), pagePath.string());
				return nullptr;
			}
			const bool nearest = page->GetProperties().SamplerFilter == TextureFilter::Nearest;
			if (data.Smooth == false && nearest == false)
			{
				HY_CORE_WARN("Font: the page '{}' isn't filtered Nearest, so scaled text will blur; set Filter to Nearest in its import settings", pagePath.string());
			}
			else if (data.Smooth && nearest)
			{
				HY_CORE_WARN("Font: '{}' is a smooth font but its page '{}' is filtered Nearest, so scaled text will look jagged; set Filter to Linear", path.filename().string(), pagePath.string());
			}
			pages.push_back(page);
		}

		HY_CORE_INFO("Font: '{}' ({} px, {} characters, {} kerning pairs)", path.filename().string(), data.Size, data.Glyphs.size(), data.Kerning.size());
		return CreateRef<Font>(data, std::move(pages));
	}

	const Ref<Texture2D>& Font::GetPage(uint32_t index) const
	{
		static const Ref<Texture2D> s_None;
		return index < m_Pages.size() ? m_Pages[index] : s_None;
	}

	bool Font::ShouldReportMissing(uint32_t codepoint) const
	{
		return m_ReportedMissing.insert(codepoint).second;
	}
}
