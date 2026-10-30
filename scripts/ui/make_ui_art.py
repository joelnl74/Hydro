"""Draw the runtime UI's frames, ribbons and markers (Oxygen/project/Assets/UI) for the 1920 x 1080 canvas (D5).

Original art in the game's own craft language (docs/StyleGuide.md, the art guide's UI chapter): dark ink surfaces,
bronze fittings, washi paper, shrine-red accents. Everything is drawn here at 4x and scaled down, so the edges are
smooth; the images are imported with linear filtering. Run it again after changing a shape:

    python scripts/ui/make_ui_art.py

Writes the PNGs and, for new ones, a .hymeta with a fresh handle (linear, clamped, single sprite). The nine-slice
borders each image is drawn for are listed in BORDERS (and docs/SceneAndECS.md, "UI art").

Needs Pillow.
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Oxygen", "project", "Assets", "UI")
S = 4  # supersampling

INK = (14, 17, 26)
INK_BLUE = (18, 30, 44)
BRONZE = (198, 160, 96)
BRONZE_DARK = (120, 90, 50)
GOLD = (236, 206, 140)
CREAM = (239, 230, 210)
PAPER_EDGE = (150, 122, 84)
VERMILION = (196, 58, 40)
STEEL = (70, 104, 150)

# Nine-slice borders (left, bottom, right, top) in texture pixels, for the scenes' UIImageComponents.
BORDERS = {
    "Ribbon": (20, 10, 34, 10),
    "RibbonFocus": (20, 10, 34, 10),
    "Frame": (22, 22, 22, 22),
    "Paper": (22, 22, 22, 22),
    "Banner": (26, 12, 26, 12),
    "Divider": (14, 0, 14, 0),
    "Plate": (16, 8, 16, 8),
}


def canvas(width, height):
    return Image.new("RGBA", (width * S, height * S), (0, 0, 0, 0))


def finish(image, name):
    small = image.resize((image.width // S, image.height // S), Image.LANCZOS)
    path = os.path.join(OUT, name + ".png")
    small.save(path)
    meta = path + ".hymeta"
    if os.path.exists(meta) is False:
        with open(meta, "w", newline="\r\n") as file:
            file.write("Handle: %d\n" % random.getrandbits(63))
            file.write("Type: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Linear\n  Wrap: Clamp\n  SpriteMode: Single\n  PixelsPerUnit: 100\n")
    print("wrote", path, small.size)


def horizontal_gradient(width, height, left, right):
    """RGBA gradient from `left` to `right` (both RGBA tuples)."""
    image = Image.new("RGBA", (width, height))
    pixels = image.load()
    for x in range(width):
        t = x / max(width - 1, 1)
        colour = tuple(int(round(a + (b - a) * t)) for a, b in zip(left, right))
        for y in range(height):
            pixels[x, y] = colour
    return image


def ribbon(name, fill_left, fill_right, line, tip):
    """A command ribbon: a band with a pointed right end, a bronze line along its edges and a fitting at the tip."""
    w, h = 256, 64
    image = canvas(w, h)
    W, H = w * S, h * S
    point = 30 * S
    shape = [(0, 6 * S), (W - point, 6 * S), (W - 4 * S, H // 2), (W - point, H - 6 * S), (0, H - 6 * S)]
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).polygon(shape, fill=255)
    fill = horizontal_gradient(W, H, fill_left, fill_right)
    image.paste(fill, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    width = int(1.6 * S)
    draw.line(shape[1:4] + [shape[3]], fill=line, width=width, joint="curve")
    draw.line([shape[0], shape[1]], fill=line, width=width)
    draw.line([shape[4], shape[3]], fill=line, width=width)
    # A short bronze cap at the left end, and the fitting at the tip: a small rhombus with a dot.
    draw.line([(3 * S, 6 * S), (3 * S, H - 6 * S)], fill=line, width=width)
    cx, cy, r = W - 10 * S, H // 2, 5 * S
    draw.polygon([(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)], fill=tip)
    draw.ellipse([cx - S, cy - S, cx + S, cy + S], fill=INK + (255,))
    finish(image, name)


def frame():
    """A dark ink panel with a double bronze border and small corner fittings."""
    w = h = 128
    image = canvas(w, h)
    W, H = w * S, h * S
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([2 * S, 2 * S, W - 2 * S, H - 2 * S], radius=6 * S, fill=INK + (215,))
    draw.rounded_rectangle([5 * S, 5 * S, W - 5 * S, H - 5 * S], radius=4 * S, outline=BRONZE + (230,), width=int(1.5 * S))
    draw.rounded_rectangle([9 * S, 9 * S, W - 9 * S, H - 9 * S], radius=3 * S, outline=BRONZE_DARK + (160,), width=S)
    for x, y, sx, sy in ((5, 5, 1, 1), (w - 5, 5, -1, 1), (5, h - 5, 1, -1), (w - 5, h - 5, -1, -1)):
        px, py = x * S, y * S
        draw.line([(px, py + sy * 14 * S), (px, py), (px + sx * 14 * S, py)], fill=GOLD + (255,), width=int(2.5 * S))
        draw.ellipse([px + sx * 5 * S - 2 * S, py + sy * 5 * S - 2 * S, px + sx * 5 * S + 2 * S, py + sy * 5 * S + 2 * S], fill=GOLD + (255,))
    finish(image, "Frame")


def paper():
    """Washi paper for dialogue: cream with soft fibres, a warm edge and chamfered corners."""
    w = h = 128
    W, H = w * S, h * S
    rng = random.Random(7)
    image = canvas(w, h)
    c = 10 * S
    shape = [(c, 2 * S), (W - c, 2 * S), (W - 2 * S, c), (W - 2 * S, H - c), (W - c, H - 2 * S), (c, H - 2 * S), (2 * S, H - c), (2 * S, c)]
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).polygon(shape, fill=255)
    base = Image.new("RGBA", (W, H), CREAM + (246,))
    fibres = ImageDraw.Draw(base)
    for _ in range(260):
        x, y = rng.uniform(0, W), rng.uniform(0, H)
        angle = rng.uniform(0, math.pi)
        length = rng.uniform(4, 18) * S
        shade = rng.randint(-14, 8)
        colour = tuple(max(0, min(255, v + shade)) for v in CREAM) + (246,)
        fibres.line([(x, y), (x + math.cos(angle) * length, y + math.sin(angle) * length)], fill=colour, width=S)
    base = base.filter(ImageFilter.GaussianBlur(S * 0.6))
    image.paste(base, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    draw.polygon(shape, outline=PAPER_EDGE + (255,), width=int(1.6 * S))
    inner = [(x + (S * 5 if x < W / 2 else -S * 5), y + (S * 5 if y < H / 2 else -S * 5)) for x, y in shape]
    draw.polygon(inner, outline=PAPER_EDGE + (90,), width=S)
    finish(image, "Paper")


def banner():
    """The skill-name banner: a deep blue band, bronze lines above and below, diamond end caps."""
    w, h = 128, 48
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    draw.rectangle([10 * S, 6 * S, W - 10 * S, H - 6 * S], fill=INK_BLUE + (215,))
    for y in (6 * S, H - 6 * S):
        draw.line([(10 * S, y), (W - 10 * S, y)], fill=BRONZE + (240,), width=int(1.6 * S))
    for cx in (10 * S, W - 10 * S):
        cy, r = H // 2, 8 * S
        draw.polygon([(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)], fill=INK_BLUE + (230,), outline=BRONZE + (255,))
        draw.polygon([(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)], outline=BRONZE + (255,), width=int(1.6 * S))
    finish(image, "Banner")


def divider():
    """A thin bronze rule with a diamond at each end (the party list, headings)."""
    w, h = 256, 12
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    cy = H // 2
    draw.line([(8 * S, cy), (W - 8 * S, cy)], fill=BRONZE + (200,), width=int(1.4 * S))
    for cx in (5 * S, W - 5 * S):
        r = 4 * S
        draw.polygon([(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)], outline=BRONZE + (230,), width=S)
    finish(image, "Divider")


def plate():
    """A small dark plate for headers and name tags: bronze top line, soft ends."""
    w, h = 96, 32
    W, H = w * S, h * S
    image = canvas(w, h)
    fill = Image.new("RGBA", (W, H), INK + (0,))
    pixels = fill.load()
    for x in range(W):
        t = min(x, W - 1 - x) / (16 * S)
        alpha = int(220 * min(1.0, t))
        for y in range(2 * S, H - 2 * S):
            pixels[x, y] = INK + (alpha,)
    image.alpha_composite(fill)
    draw = ImageDraw.Draw(image)
    draw.line([(12 * S, 2 * S), (W - 12 * S, 2 * S)], fill=BRONZE + (230,), width=int(1.4 * S))
    finish(image, "Plate")


def cursor():
    """The focus cursor: a vermilion diamond with a gold rim and a short brush-stroke tail, pointing right."""
    w = h = 48
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    cx, cy = 30 * S, H // 2
    # the tail: a tapering stroke
    for i in range(18 * S):
        t = i / (18 * S)
        r = (1 - t) * 3.5 * S
        x = cx - 8 * S - i
        draw.ellipse([x - r, cy - r, x + r, cy + r], fill=VERMILION + (int(230 * (1 - t)),))
    r = 11 * S
    diamond = [(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)]
    draw.polygon(diamond, fill=VERMILION + (255,))
    draw.polygon(diamond, outline=GOLD + (255,), width=int(2.2 * S))
    draw.ellipse([cx - 3 * S, cy - 3 * S, cx + 3 * S, cy + 3 * S], fill=GOLD + (255,))
    finish(image, "Cursor")


def marker():
    """The dialogue's "next" marker: a small vermilion diamond pointing down."""
    w = h = 32
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    cx, cy = W // 2, H // 2
    draw.polygon([(cx - 9 * S, cy - 6 * S), (cx + 9 * S, cy - 6 * S), (cx, cy + 10 * S)], fill=VERMILION + (255,), outline=BRONZE_DARK + (255,))
    finish(image, "Marker")


def shield():
    """An enemy's shield badge (the number goes on top): steel blue, bronze rim, a rounded crest."""
    w, h = 64, 72
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    top, mid, bottom = 6 * S, 40 * S, H - 4 * S
    left, right = 8 * S, W - 8 * S
    outline = [(left, top + 6 * S), (W // 2, top), (right, top + 6 * S), (right, mid)]
    for i in range(21):
        t = i / 20
        x = right + (W // 2 - right) * t
        y = mid + (bottom - mid) * math.sin(t * math.pi / 2)
        outline.append((x, y))
    for i in range(21):
        t = i / 20
        x = W // 2 + (left - W // 2) * t
        y = bottom - (bottom - mid) * (1 - math.cos(t * math.pi / 2))
        outline.append((x, y))
    draw.polygon(outline, fill=STEEL + (240,))
    draw.line(outline + [outline[0]], fill=GOLD + (255,), width=int(2.4 * S), joint="curve")
    # A soft sheen on the left half, blended on its own layer.
    sheen = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(sheen).polygon([(left + 5 * S, top + 10 * S), (W // 2, top + 5 * S), (W // 2, mid), (left + 5 * S, mid - 6 * S)], fill=(255, 255, 255, 46))
    image.alpha_composite(sheen)
    finish(image, "Shield")


def chip():
    """A turn-order chip: a dark diamond with a bronze rim (the actor's sprite goes on top)."""
    w = h = 64
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    cx, cy, r = W // 2, H // 2, 29 * S
    diamond = [(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)]
    draw.polygon(diamond, fill=INK_BLUE + (225,))
    draw.polygon(diamond, outline=BRONZE + (255,), width=int(2 * S))
    r2 = r - 5 * S
    draw.polygon([(cx - r2, cy), (cx, cy - r2), (cx + r2, cy), (cx, cy + r2)], outline=BRONZE_DARK + (180,), width=S)
    finish(image, "Chip")


def pip():
    """A boost point: a small solid diamond with a light rim (tinted by the script: full, empty, about to be spent)."""
    w = h = 24
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    cx, cy, r = W // 2, H // 2, 10 * S
    diamond = [(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)]
    draw.polygon(diamond, fill=(255, 255, 255, 255))
    draw.polygon(diamond, outline=(120, 120, 120, 255), width=int(1.5 * S))
    finish(image, "Pip")


def gradient_bar():
    """A soft horizontal fade (ink, opaque on the left): behind status text and the help bar."""
    w, h = 256, 16
    image = horizontal_gradient(w * S, h * S, INK + (200,), INK + (0,))
    finish(image, "Fade")


def quest_mark():
    """The quest marker over an NPC: a white diamond badge with an ink "!" and a short point below. Drawn white so
    the script tints it (vermilion: a new errand, gold: come back to me)."""
    w, h = 64, 80
    W, H = w * S, h * S
    image = canvas(w, h)
    draw = ImageDraw.Draw(image)
    cx, cy, r = W // 2, 30 * S, 26 * S
    diamond = [(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)]
    # the point below the badge
    draw.polygon([(cx - 7 * S, cy + r - 6 * S), (cx + 7 * S, cy + r - 6 * S), (cx, H - 4 * S)], fill=(236, 230, 220, 255))
    draw.polygon(diamond, fill=(244, 240, 232, 255))
    draw.polygon(diamond, outline=BRONZE_DARK + (255,), width=int(2.6 * S))
    r2 = r - 6 * S
    draw.polygon([(cx - r2, cy), (cx, cy - r2), (cx + r2, cy), (cx, cy + r2)], outline=(255, 255, 255, 200), width=S)
    # the "!": a tapering stroke and a dot
    top, bottom = cy - 15 * S, cy + 5 * S
    draw.polygon([(cx - 4 * S, top), (cx + 4 * S, top), (cx + 2 * S, bottom), (cx - 2 * S, bottom)], fill=INK + (255,))
    draw.ellipse([cx - 3 * S, cy + 9 * S, cx + 3 * S, cy + 15 * S], fill=INK + (255,))
    finish(image, "QuestMark")


def band():
    """The quest banner's band: soft ink across the middle of the screen, fading out to both ends and its edges
    (stretched to any size, no nine-slice)."""
    w, h = 512, 96
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    pixels = image.load()
    for x in range(w):
        u = abs(x / (w - 1) * 2.0 - 1.0)            # 0 in the middle, 1 at the ends
        across = max(0.0, 1.0 - u ** 2.2)
        for y in range(h):
            v = abs(y / (h - 1) * 2.0 - 1.0)
            edge = min(1.0, (1.0 - v) / 0.35)          # soft top and bottom
            alpha = int(round(205 * across * edge))
            pixels[x, y] = INK + (alpha,)
    # finish() scales down by S: draw at full size, then enlarge so it comes back the same.
    finish(image.resize((w * S, h * S), Image.BICUBIC), "Band")


def main():
    os.makedirs(OUT, exist_ok=True)
    ribbon("Ribbon", INK + (225,), INK_BLUE + (150,), BRONZE_DARK + (220,), BRONZE + (255,))
    ribbon("RibbonFocus", (92, 70, 38, 235), (60, 48, 30, 170), GOLD + (255,), VERMILION + (255,))
    frame()
    paper()
    banner()
    divider()
    plate()
    cursor()
    marker()
    shield()
    chip()
    pip()
    gradient_bar()
    quest_mark()
    band()


if __name__ == "__main__":
    main()
