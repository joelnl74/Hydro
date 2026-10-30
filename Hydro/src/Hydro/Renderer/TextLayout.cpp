#include "hypch.h"
#include "TextLayout.h"

#include "Hydro/Renderer/Font.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	static constexpr uint32_t s_Replacement = 0xFFFD;

	uint32_t TextLayout::DecodeUTF8(const std::string& text, size_t& position)
	{
		const size_t length = text.size();
		const uint8_t first = (uint8_t)text[position];
		uint32_t codepoint = 0;
		size_t extra = 0;
		if (first < 0x80)
		{
			position++;
			return first;
		}
		if ((first & 0xE0) == 0xC0)
		{
			codepoint = first & 0x1F;
			extra = 1;
		}
		else if ((first & 0xF0) == 0xE0)
		{
			codepoint = first & 0x0F;
			extra = 2;
		}
		else if ((first & 0xF8) == 0xF0)
		{
			codepoint = first & 0x07;
			extra = 3;
		}
		else
		{
			position++;
			return s_Replacement;
		}

		if (position + extra >= length)
		{
			position++;
			return s_Replacement;
		}
		for (size_t i = 1; i <= extra; i++)
		{
			const uint8_t next = (uint8_t)text[position + i];
			if ((next & 0xC0) != 0x80)
			{
				position++;
				return s_Replacement;
			}
			codepoint = (codepoint << 6) | (next & 0x3F);
		}
		position += extra + 1;
		return codepoint;
	}

	// The glyph a character draws with: its own, else '?' (reported once per font), else none.
	static const FontGlyph* ResolveGlyph(const FontData& data, uint32_t codepoint, const Font* font)
	{
		if (const FontGlyph* glyph = data.FindGlyph(codepoint))
		{
			return glyph;
		}
		if (font != nullptr && font->ShouldReportMissing(codepoint))
		{
			HY_CORE_WARN("Font '{}': no character U+{:04X}; drawn as '?'", data.Face, codepoint);
		}
		return data.FindGlyph('?');
	}

	// The advance of characters [begin, end) in font pixels, kerning included.
	static float Measure(const std::vector<uint32_t>& codepoints, size_t begin, size_t end, const FontData& data)
	{
		float width = 0.0f;
		uint32_t previous = 0;
		for (size_t i = begin; i < end; i++)
		{
			const uint32_t codepoint = codepoints[i];
			const FontGlyph* glyph = data.FindGlyph(codepoint);
			if (glyph == nullptr)
			{
				glyph = data.FindGlyph('?');
			}
			if (previous != 0)
			{
				width += data.GetKerning(previous, codepoint);
			}
			width += glyph != nullptr ? glyph->Advance : 0.0f;
			previous = codepoint;
		}
		return width;
	}

	void TextLayout::Layout(const std::string& text, const FontData& data, const TextLayoutProperties& properties, TextLayoutResult& result, const Font* font)
	{
		result.Glyphs.clear();
		result.Size = { 0.0f, 0.0f };
		result.LineCount = 0;
		result.CharacterCount = 0;
		if (text.empty())
		{
			return;
		}

		const float scale = (float)std::max(properties.Scale, 1u);
		const float maxWidth = properties.Wrap && properties.BoxWidth > 0.0f ? properties.BoxWidth / scale : 0.0f; // font pixels

		std::vector<uint32_t> codepoints;
		codepoints.reserve(text.size());
		for (size_t position = 0; position < text.size();)
		{
			const uint32_t codepoint = DecodeUTF8(text, position);
			if (codepoint != '\r')
			{
				codepoints.push_back(codepoint);
			}
		}

		// Lines as [begin, end) ranges; spaces where a line was wrapped belong to no line.
		struct Line
		{
			size_t Begin = 0;
			size_t End = 0;
			float Width = 0.0f; // font pixels, trailing spaces left out
		};
		std::vector<Line> lines;
		const auto addLine = [&](size_t begin, size_t end)
		{
			size_t trimmed = end;
			while (trimmed > begin && codepoints[trimmed - 1] == ' ')
			{
				trimmed--;
			}
			lines.push_back({ begin, end, Measure(codepoints, begin, trimmed, data) });
		};

		size_t paragraphBegin = 0;
		while (paragraphBegin <= codepoints.size())
		{
			size_t paragraphEnd = paragraphBegin;
			while (paragraphEnd < codepoints.size() && codepoints[paragraphEnd] != '\n')
			{
				paragraphEnd++;
			}

			size_t lineBegin = paragraphBegin;
			if (maxWidth > 0.0f)
			{
				size_t i = paragraphBegin;
				while (i < paragraphEnd)
				{
					// Does the line still fit with character i on it? Spaces always fit (they may hang off the end).
					if (codepoints[i] != ' ' && i > lineBegin && Measure(codepoints, lineBegin, i + 1, data) > maxWidth)
					{
						size_t lastSpace = i;
						while (lastSpace > lineBegin && codepoints[lastSpace - 1] != ' ')
						{
							lastSpace--;
						}
						if (lastSpace > lineBegin)
						{
							// Break after the last space before this word: the word moves to the next line.
							size_t lineEnd = lastSpace - 1;
							while (lineEnd > lineBegin && codepoints[lineEnd - 1] == ' ')
							{
								lineEnd--;
							}
							addLine(lineBegin, lineEnd);
							lineBegin = lastSpace;
						}
						else
						{
							// One word wider than the box: break it here.
							addLine(lineBegin, i);
							lineBegin = i;
						}
						continue; // measure character i again on the new line
					}
					i++;
				}
			}
			addLine(lineBegin, paragraphEnd);
			paragraphBegin = paragraphEnd + 1;
		}

		float widest = 0.0f;
		for (const Line& line : lines)
		{
			widest = std::max(widest, line.Width * scale);
		}
		const float boxWidth = properties.BoxWidth > 0.0f ? properties.BoxWidth : widest;
		const float lineAdvance = std::round(data.LineHeight * std::max(properties.LineSpacing, 0.0f)) * scale;

		uint32_t characterIndex = 0;
		for (size_t lineIndex = 0; lineIndex < lines.size(); lineIndex++)
		{
			const Line& line = lines[lineIndex];
			const float lineWidth = line.Width * scale;
			float x = 0.0f;
			if (properties.AlignH == TextAlignH::Center)
			{
				x = std::floor((boxWidth - lineWidth) * 0.5f);
			}
			else if (properties.AlignH == TextAlignH::Right)
			{
				x = boxWidth - lineWidth;
			}
			const float y = (float)lineIndex * lineAdvance;

			uint32_t previous = 0;
			for (size_t i = line.Begin; i < line.End; i++)
			{
				const uint32_t codepoint = codepoints[i];
				const FontGlyph* glyph = ResolveGlyph(data, codepoint, font);
				if (previous != 0)
				{
					x += data.GetKerning(previous, codepoint) * scale;
				}
				previous = codepoint;
				if (glyph == nullptr)
				{
					continue;
				}

				if (glyph->SourceSize.x > 0.0f && glyph->SourceSize.y > 0.0f)
				{
					TextGlyphQuad quad;
					quad.Min = { x + glyph->Offset.x * scale, y + glyph->Offset.y * scale };
					quad.Max = quad.Min + glyph->SourceSize * scale;
					quad.SourceMin = glyph->SourcePosition;
					quad.SourceMax = glyph->SourcePosition + glyph->SourceSize;
					quad.Page = glyph->Page;
					quad.CharacterIndex = characterIndex;
					result.Glyphs.push_back(quad);
				}
				if (codepoint != ' ')
				{
					characterIndex++;
				}
				x += glyph->Advance * scale;
			}
		}

		result.LineCount = (uint32_t)lines.size();
		result.CharacterCount = characterIndex;
		result.Size = { boxWidth, lines.empty() ? 0.0f : (float)(lines.size() - 1) * lineAdvance + data.LineHeight * scale };
	}
}
