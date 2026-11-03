#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	struct FontData;
	class Font;

	enum class TextAlignH : uint8_t { Left = 0, Center, Right };
	enum class TextAlignV : uint8_t { Top = 0, Middle, Bottom };

	struct TextLayoutProperties
	{
		uint32_t Scale = 1;              // whole multiples of the font's pixels
		float BoxWidth = 0.0f;           // the width lines are aligned in (and wrapped at); 0 = the widest line
		bool Wrap = true;                // break lines at BoxWidth, on spaces (a word wider than the box breaks anywhere)
		TextAlignH AlignH = TextAlignH::Left;
		float LineSpacing = 1.0f;        // times the font's line height
	};

	// One glyph to draw, in pixels from the text's top-left corner (Y down), and where it is in its font page.
	struct TextGlyphQuad
	{
		glm::vec2 Min{ 0.0f, 0.0f };          // top-left
		glm::vec2 Max{ 0.0f, 0.0f };          // bottom-right
		glm::vec2 SourceMin{ 0.0f, 0.0f };    // page pixels, Y down
		glm::vec2 SourceMax{ 0.0f, 0.0f };
		uint32_t Page = 0;
		uint32_t CharacterIndex = 0;          // how many visible characters come before it (UITextComponent::VisibleCharacters)
	};

	struct TextLayoutResult
	{
		std::vector<TextGlyphQuad> Glyphs;
		glm::vec2 Size{ 0.0f, 0.0f };         // the text's box: BoxWidth (or the widest line) by its lines' height
		uint32_t LineCount = 0;
		uint32_t CharacterCount = 0;          // characters that count for VisibleCharacters (not spaces or line breaks)
	};

	// Lays text out in a bitmap font, in whole pixels: UTF-8, line breaks, wrapping, kerning, horizontal alignment. The
	// layout never depends on how much is shown, so a typewriter reveal never moves anything.
	class TextLayout
	{
	public:
		// Clears and fills `result` (keeping its capacity). Missing characters draw as '?'; with `font`, each is logged once.
		static void Layout(const std::string& text, const FontData& data, const TextLayoutProperties& properties, TextLayoutResult& result,
			const Font* font = nullptr);
		// The next code point at `position` (moved past it). A malformed sequence gives U+FFFD and moves one byte.
		static uint32_t DecodeUTF8(const std::string& text, size_t& position);
	};
}
