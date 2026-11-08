"""python scripts/game/japanese_city/gen_temple_tex.py <Assets/Textures/Ground/Grass.png> <art_incoming/jp_city/temple_r1>
       [<art_incoming/jp_city/step12_prepared/JP_Decal_DirtPatch.png>]

Temple ground textures (Nov 2): mossy temple grass (a recolour of Grass.png), a ragged grass edge strip, a thin
creeping strip for path sides, and dark / light grass patches. Lineless; colours only from the grass's own palette."""
import colorsys, math, random, sys
from PIL import Image

SRC = sys.argv[1]
OUT = sys.argv[2]
DIRT = sys.argv[3] if len(sys.argv) > 3 else None   # step12_prepared/JP_Decal_DirtPatch.png: a centred copy is made
grass = Image.open(SRC).convert("RGBA")
W, H = grass.size


def recolour(c, hue=0.025, sat=0.8, val=0.8):
    r, g, b, a = c
    h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    h = (h + hue) % 1.0
    s = min(1.0, s * sat)
    v = min(1.0, v * val)
    r, g, b = colorsys.hsv_to_rgb(h, s, v)
    return (int(r * 255 + 0.5), int(g * 255 + 0.5), int(b * 255 + 0.5), a)


cache = {}
def rc(c, **k):
    key = (c, tuple(sorted(k.items())))
    if key not in cache:
        cache[key] = recolour(c, **k)
    return cache[key]


temple = Image.new("RGBA", (W, H))
tp, gp = temple.load(), grass.load()
for y in range(H):
    for x in range(W):
        tp[x, y] = rc(gp[x, y])
temple.save(OUT + "/JP_Ground_TempleGrass.png")

# the palette, darkest to lightest (for highlights at tuft tips)
pal = sorted(set(tp[x, y] for x in range(0, W, 2) for y in range(0, H, 2)), key=lambda c: c[0] * 0.3 + c[1] * 0.59 + c[2] * 0.11)
light = pal[int(len(pal) * 0.8):]
dark = pal[: int(len(pal) * 0.25)]
SHADOW = (74, 62, 48, 255)     # a dark earth tone under the tufts (not an outline: the grass's cast shadow on the dirt)


def periodic_noise(n, period, amp, rnd):
    """1-D value noise, tileable over n pixels."""
    k = max(1, n // period)
    pts = [rnd.uniform(-1, 1) for _ in range(k)]
    out = []
    for x in range(n):
        f = x / n * k
        i = int(f)
        t = f - i
        t = t * t * (3 - 2 * t)
        out.append(amp * (pts[i % k] * (1 - t) + pts[(i + 1) % k] * t))
    return out


def edge_profile(n, base, rnd, amp=(3.0, 1.5), tuft=(2, 5)):
    a = periodic_noise(n, 32, amp[0], rnd)
    b = periodic_noise(n, 8, amp[1], rnd)
    h = [base + a[x] + b[x] for x in range(n)]
    x = rnd.randint(0, 6)
    while x < n:                                           # tufts: 1-2 px wide spikes
        w = rnd.choice((1, 1, 2))
        tall = rnd.randint(*tuft)
        for i in range(w):
            h[(x + i) % n] += tall - i
        x += rnd.randint(4, 9)
    return [int(round(v)) for v in h]


def strip(w, h, top_base, bottom_base, seed, src_off=(0, 0), top_tufts=(1, 3), bottom_tufts=(2, 5)):
    """A horizontally tileable strip of temple grass: ragged at the bottom (towards the dirt) and the top (into the
    grass sheet, where a ragged seam hides the texture phase change)."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    p = img.load()
    bot = edge_profile(w, bottom_base, rnd, tuft=bottom_tufts)
    top = [h - 1 - v for v in edge_profile(w, h - 1 - top_base, rnd, amp=(2.0, 1.0), tuft=top_tufts)]
    for x in range(w):
        y0, y1 = max(0, top[x]), min(h - 1, bot[x])
        for y in range(y0, y1 + 1):
            c = tp[(x + src_off[0]) % W, (y + src_off[1]) % H]
            if y1 - y <= 1 and rnd.random() < 0.7:          # sunlit tips along the outer edge
                c = rnd.choice(light)
            elif y - y0 <= 0 and rnd.random() < 0.5:
                c = rnd.choice(light)
            p[x, y] = c
        if y1 + 1 < h and rnd.random() < 0.8:               # the grass's shadow just past the edge
            p[x, y1 + 1] = SHADOW
    return img


# A decal box's top face maps UVs from the box's centre (MeshFactory: u = x, v = -y, in metres times the tiling), so
# a box exactly one image in size shows the image cut through the middle with its halves swapped. Saved rolled by
# half an image the other way, the box shows it whole: the strips' grass towards the box's +Y edge (the builders turn
# +Y into the lawn), the patches centred (Nov 2).
def rolled(img, dx, dy):
    w, h = img.size
    out = Image.new("RGBA", (w, h))
    for (ox, oy) in ((0, 0), (-w, 0), (0, -h), (-w, -h)):
        out.paste(img, (ox + dx, oy + dy))
    return out


rolled(strip(256, 32, 4, 20, 3, (17, 41)), 0, 16).save(OUT + "/JP_Decal_GrassEdge.png")
rolled(strip(256, 16, 3, 9, 5, (101, 7), top_tufts=(2, 4), bottom_tufts=(1, 4)), 0, 8).save(OUT + "/JP_Decal_GrassCreep.png")


def patch(size, seed, tone):
    """A ragged blob of grass a shade darker (moss) or lighter (dry), for breaking up the tiling."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    p = img.load()
    n = 48
    radii = periodic_noise(n * 4, 24, 0.18, rnd)
    radii2 = periodic_noise(n * 4, 6, 0.06, rnd)
    cx = cy = size / 2
    for y in range(size):
        for x in range(size):
            dx, dy = (x - cx) / (size / 2), (y - cy) / (size / 2)
            ang = (math.atan2(dy, dx) / (2 * math.pi)) % 1.0
            i = int(ang * len(radii)) % len(radii)
            r = 0.78 + radii[i] + radii2[i]
            d = math.hypot(dx, dy * 1.15)
            if d < r:
                c = tp[(x * 3 + seed * 13) % W, (y * 3 + seed * 7) % H] if False else tp[(x + seed * 37) % W, (y + seed * 11) % H]
                if tone == "dark":
                    c = rc(c, hue=0.02, sat=0.95, val=0.82)
                else:
                    c = rc(c, hue=-0.035, sat=0.85, val=1.12)
                if r - d < 0.06 and rnd.random() < 0.55:      # the rim breaks up into single pixels
                    continue
                p[x, y] = c
    return img


rolled(patch(128, 1, "dark"), 64, 64).save(OUT + "/JP_Decal_GrassPatch_Dark.png")
rolled(patch(128, 2, "light"), 64, 64).save(OUT + "/JP_Decal_GrassPatch_Light.png")
if DIRT:
    d = Image.open(DIRT).convert("RGBA")
    rolled(d, d.size[0] // 2, d.size[1] // 2).save(OUT + "/JP_Decal_DirtPatch_C.png")
print("ok", len(pal))
