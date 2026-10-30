"""Bake a TTF / OTF font into an AngelCode BMFont (text .fnt + one PNG page) for Hydro's runtime UI.

Pixel fonts are rasterised without anti-aliasing at their native size, so every glyph stays on whole pixels;
draw them at whole-number scales (UITextComponent::Scale). Smooth fonts (--smooth: serif or sans text for a
high-resolution canvas) are anti-aliased and their page is imported with linear filtering. Glyphs are white with
alpha (the UI tints them).

Usage:
    python scripts/fonts/make_bmfont.py <font.ttf> --size 11 --out Oxygen/project/Assets/Fonts/PixelifySans11
        [--chars ascii,latin1,punctuation] [--padding 1] [--page-width 256] [--smooth]

Writes <out>.fnt, <out>.png and, when they don't exist yet, <out>.png.hymeta (nearest filtering for a pixel font,
linear for a smooth one; clamped) and <out>.fnt.hymeta with fresh asset handles. Copy the font's licence next to it.

Needs Pillow and fontTools (pip install pillow fonttools).
"""

import argparse
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

CHARACTER_SETS = {
    "ascii": list(range(0x20, 0x7F)),
    "latin1": list(range(0xA0, 0x100)),
    # Typographic quotes, dashes and the ellipsis that dialogue tends to use.
    "punctuation": [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026],
}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("font", help="the .ttf / .otf file")
    parser.add_argument("--size", type=int, required=True, help="pixel size (a pixel font's native size)")
    parser.add_argument("--out", required=True, help="output path without extension")
    parser.add_argument("--chars", default="ascii,latin1,punctuation", help="comma-separated sets: " + ", ".join(CHARACTER_SETS))
    parser.add_argument("--padding", type=int, default=1, help="empty pixels around each glyph in the page")
    parser.add_argument("--page-width", type=int, default=256, help="raise it for large sizes (512 or 1024)")
    parser.add_argument("--smooth", action="store_true", help="anti-aliased glyphs (for non-pixel fonts)")
    return parser.parse_args()


def write_hymeta(path, lines):
    if os.path.exists(path):
        return
    with open(path, "w", newline="\r\n") as file:
        file.write("Handle: %d\n" % random.getrandbits(63))
        for line in lines:
            file.write(line + "\n")


def main():
    args = parse_args()
    cmap = TTFont(args.font).getBestCmap()
    font = ImageFont.truetype(args.font, args.size)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent

    codepoints = []
    for name in args.chars.split(","):
        if name not in CHARACTER_SETS:
            sys.exit("unknown character set '%s'" % name)
        codepoints += [c for c in CHARACTER_SETS[name] if c in cmap and c not in codepoints]
    if 0x20 not in codepoints:
        codepoints.insert(0, 0x20)

    # Rasterise each glyph on its own, cropped to its ink.
    glyphs = []
    for codepoint in codepoints:
        character = chr(codepoint)
        advance = int(round(font.getlength(character)))
        # Drawn with the pen at (margin, margin) = the line's top-left (ascender), then cropped to the pixels
        # actually set: the outline's box can be wider than its ink once it is snapped to pixels.
        margin = args.size
        canvas = Image.new("L", (args.size * 4 + margin * 2, line_height + margin * 2), 0)
        draw = ImageDraw.Draw(canvas)
        draw.fontmode = "L" if args.smooth else "1"
        draw.text((margin, margin), character, font=font, fill=255)
        box = canvas.getbbox()
        image, xoffset, yoffset = None, 0, ascent
        if box is not None:
            image = canvas.crop(box)
            xoffset, yoffset = box[0] - margin, box[1] - margin
        glyphs.append({"id": codepoint, "image": image, "xoffset": xoffset, "yoffset": yoffset, "advance": advance})

    # Shelf packing, tallest first.
    pad = args.padding
    order = sorted((g for g in glyphs if g["image"] is not None), key=lambda g: -g["image"].height)
    x, y, shelf = pad, pad, 0
    for glyph in order:
        w, h = glyph["image"].size
        if x + w + pad > args.page_width:
            x, y, shelf = pad, y + shelf + pad, 0
        glyph["x"], glyph["y"] = x, y
        x += w + pad
        shelf = max(shelf, h)
    page_height = 1
    while page_height < y + shelf + pad:
        page_height *= 2

    page = Image.new("RGBA", (args.page_width, page_height), (255, 255, 255, 0))
    for glyph in order:
        mask = glyph["image"]
        white = Image.new("RGBA", mask.size, (255, 255, 255, 255))
        white.putalpha(mask)
        page.paste(white, (glyph["x"], glyph["y"]))
    page_path = args.out + ".png"
    page.save(page_path)

    # Kerning: what drawing a pair shifts by compared with the two advances.
    kernings = []
    ink = [g for g in glyphs if g["id"] < 0x250]
    for first in ink:
        for second in ink:
            pair = chr(first["id"]) + chr(second["id"])
            amount = int(round(font.getlength(pair) - font.getlength(chr(first["id"])) - font.getlength(chr(second["id"]))))
            if amount != 0:
                kernings.append((first["id"], second["id"], amount))

    face = os.path.splitext(os.path.basename(args.font))[0]
    lines = [
        'info face="%s" size=%d bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=%d aa=1 padding=0,0,0,0 spacing=%d,%d' % (face, args.size, 1 if args.smooth else 0, pad, pad),
        "common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=1 packed=0" % (line_height, ascent, args.page_width, page_height),
        'page id=0 file="%s"' % os.path.basename(page_path),
        "chars count=%d" % len(glyphs),
    ]
    for glyph in glyphs:
        has_ink = glyph["image"] is not None
        lines.append("char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=0 chnl=15" % (
            glyph["id"], glyph.get("x", 0) if has_ink else 0, glyph.get("y", 0) if has_ink else 0,
            glyph["image"].width if has_ink else 0, glyph["image"].height if has_ink else 0,
            glyph["xoffset"], glyph["yoffset"], glyph["advance"]))
    lines.append("kernings count=%d" % len(kernings))
    for first, second, amount in kernings:
        lines.append("kerning first=%d second=%d amount=%d" % (first, second, amount))

    with open(args.out + ".fnt", "w", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")

    write_hymeta(page_path + ".hymeta", ["Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Linear" if args.smooth else "  Filter: Nearest", "  Wrap: Clamp", "  SpriteMode: Single", "  PixelsPerUnit: 32"])
    write_hymeta(args.out + ".fnt.hymeta", ["Type: Font"])
    print("%s.fnt: %d glyphs, %d kerning pairs, line height %d, base %d, page %dx%d" % (args.out, len(glyphs), len(kernings), line_height, ascent, args.page_width, page_height))


if __name__ == "__main__":
    main()
