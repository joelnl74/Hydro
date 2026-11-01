"""Draws the few sprites the Japanese countryside needs that the project didn't have (Pillow, 32 pixels per metre, in
the style of the Plants and Trees sheets: dark outline, three or four tones, light from the upper left):

  Sprites/Trees/BambooA.png, BambooB.png, BambooC.png  bamboo clumps (2 to 3 m wide, 8 to 10 m tall): stalks with
                                                       node rings, leaf sprays at the top and along the culms
  Sprites/Props/CampTarp.png                            the ronin camp's shelter: an A-frame of bamboo poles lashed
                                                        at the top, an indigo cloth thrown over the ridge pole
  Sprites/Props/KitsuneStatue.png                       a stone fox on a plinth with a red bib (the fox shrine's
                                                        guardians; FlipX makes the pair)
  Sprites/Props/BambooShoot.png                         a young bamboo shoot breaking the ground (gathered)
  Sprites/Props/LeyRift.png, LeyRiftGlow.png            the opening into the leyline by the ancient tree, flat on
                                                        the ground: steps down into the dark between split roots,
                                                        and its glowing veins on their own (drawn emissive, faded in)

    python scripts/japanese_city/make_countryside_art.py

Writes a .hymeta next to each new file (once; the handle is kept on later runs).
"""

import hashlib
import math
import os
import random

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.abspath(os.path.join(HERE, "..", "..", "Oxygen", "project", "Assets"))

OUTLINE = (32, 38, 24, 255)
STALK = [(78, 110, 46, 255), (112, 146, 60, 255), (150, 176, 82, 255), (186, 202, 112, 255)]
NODE = (58, 80, 36, 255)
LEAF = [(40, 74, 34, 255), (62, 104, 44, 255), (96, 140, 58, 255), (140, 176, 80, 255)]


def meta(path, name):
    target = path + ".hymeta"
    if os.path.exists(target):
        return
    handle = int(hashlib.sha256(("Countryside:art:" + name).encode()).hexdigest()[:16], 16) & 0x7FFFFFFFFFFFFFFF
    with open(target, "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: SRGB\n  Filter: Nearest\n  Wrap: Clamp\n"
                "  SpriteMode: Single\n  PixelsPerUnit: 32\n" % handle)


def leaf_spray(d, x, y, rnd, count, spread, droop):
    """Long thin leaves fanning out from (x, y) and drooping: each a tapered blade (3 px at its base, 1 at its tip) in
    one of the leaf tones, outlined underneath."""
    for _ in range(count):
        angle = rnd.uniform(-spread, spread) + (math.pi if rnd.random() < 0.5 else 0.0)
        length = rnd.uniform(12, 22)
        tone = LEAF[rnd.randrange(1, len(LEAF))]
        px, py = float(x), float(y)
        for k in range(int(length)):
            t = k / length
            px += math.cos(angle)
            py += math.sin(angle) * 0.3 + droop * t
            w = 3 if t < 0.3 else (2 if t < 0.7 else 1)
            d.rectangle([round(px), round(py), round(px), round(py) + w - 1], fill=tone)
            d.point((round(px), round(py) + w), fill=LEAF[0])


def bamboo(width, height, stalks, seed):
    rnd = random.Random(seed)
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    base = height - 2
    xs = sorted(rnd.uniform(width * 0.22, width * 0.78) for _ in range(stalks))
    tops = []
    for n, x0 in enumerate(xs):
        lean = rnd.uniform(-0.06, 0.06)
        top = rnd.uniform(height * 0.06, height * 0.22)
        w = rnd.choice((4, 4, 5))
        light = 1 if n % 2 else 0
        y = base
        node_gap = rnd.randint(20, 26)
        next_node = base - rnd.randint(6, node_gap)
        while y > top:
            x = x0 + lean * (base - y)
            xi = round(x)
            # outline, dark side, body, highlight
            d.rectangle([xi - 1, y, xi + w, y], fill=OUTLINE)
            d.rectangle([xi, y, xi + w - 1, y], fill=STALK[1 + light])
            d.point((xi + w - 1, y), fill=STALK[0])
            d.point((xi + 1, y), fill=STALK[2 + light])
            if y <= next_node:
                d.rectangle([xi - 1, y, xi + w, y + 1], fill=NODE)
                d.point((xi + 1, y - 1), fill=STALK[3])
                if rnd.random() < 0.6 and y < base - 40:
                    leaf_spray(d, xi + w // 2, y, rnd, 5, 0.45, 0.3)
                next_node = y - node_gap
            y -= 1
        tops.append((round(x0 + lean * (base - top)), round(top)))
    # the crowns: dense sprays round the tops, darker inside
    for (x, y) in tops:
        for _ in range(7):
            leaf_spray(d, x + rnd.randint(-4, 4), y + rnd.randint(0, 34), rnd, 12, 0.9, 0.5)
    # a little ground clutter: dry sheaths and shoots at the foot
    for _ in range(stalks * 2):
        x = rnd.randint(int(width * 0.2), int(width * 0.8))
        d.rectangle([x, base - 2, x + 2, base], fill=(150, 128, 78, 255))
        d.point((x + 1, base - 3), fill=OUTLINE)
    return img


def camp_tarp():
    """An A-frame shelter seen from the front-left: two crossed pole pairs, a ridge pole, the cloth over it."""
    w, h = 128, 104
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pole = (164, 132, 70, 255)
    pole_dark = (112, 86, 44, 255)
    cloth = [(36, 42, 74, 255), (52, 60, 98, 255), (70, 80, 124, 255), (96, 104, 148, 255)]
    # back frame (smaller, higher: further away)
    back_l, back_r, back_top = (40, 78), (100, 78), (70, 22)
    front_l, front_r, front_top = (14, 100), (78, 100), (46, 34)
    # cloth: the far slope (darker), then the near slope, hanging over the ridge from back_top to front_top
    d.polygon([back_top, front_top, front_r, back_r], fill=cloth[0], outline=OUTLINE)
    d.polygon([back_top, front_top, front_l, (back_l[0] + 6, back_l[1])], fill=cloth[2], outline=OUTLINE)
    # stripes and a patch on the near slope
    for k in range(1, 5):
        t = k / 5.0
        a = (back_top[0] + (back_l[0] + 6 - back_top[0]) * t, back_top[1] + (back_l[1] - back_top[1]) * t)
        b = (front_top[0] + (front_l[0] - front_top[0]) * t, front_top[1] + (front_l[1] - front_top[1]) * t)
        d.line([a, b], fill=cloth[1 if k % 2 else 3], width=1)
    d.polygon([(30, 70), (40, 64), (46, 76), (36, 82)], fill=cloth[3], outline=cloth[0])
    # the opening under the near frame: dark inside
    d.polygon([front_top, (front_l[0] + 8, front_l[1]), (front_r[0] - 8, front_r[1])], fill=(18, 20, 30, 255))
    # poles: front pair crossed over the top, ridge pole, back pair
    for (a, b) in ((front_l, (front_top[0] + 6, front_top[1] - 6)), (front_r, (front_top[0] - 6, front_top[1] - 6)),
                   (back_l, (back_top[0] + 5, back_top[1] - 5)), (back_r, (back_top[0] - 5, back_top[1] - 5))):
        d.line([a, b], fill=OUTLINE, width=4)
        d.line([a, b], fill=pole, width=2)
    d.line([(front_top[0] - 2, front_top[1]), (back_top[0] + 2, back_top[1])], fill=OUTLINE, width=4)
    d.line([(front_top[0] - 2, front_top[1]), (back_top[0] + 2, back_top[1])], fill=pole_dark, width=2)
    # lashings
    for (x, y) in (front_top, back_top):
        d.rectangle([x - 2, y - 2, x + 2, y + 1], fill=(196, 170, 110, 255), outline=OUTLINE)
    return img


def kitsune_statue():
    """A seated stone fox on a plinth, facing front-left: ears up, a red bib, a lighter stone where the light falls."""
    w, h = 32, 50
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    stone = [(74, 76, 74, 255), (110, 112, 106, 255), (146, 146, 136, 255), (184, 182, 168, 255)]
    moss = (88, 110, 58, 255)
    # plinth
    d.rectangle([4, 38, 27, 48], fill=stone[1], outline=OUTLINE)
    d.rectangle([5, 39, 26, 41], fill=stone[2])
    d.line([(5, 47), (26, 47)], fill=stone[0])
    d.point([(8, 44), (20, 42), (23, 46)], fill=moss)
    # body: haunches and chest
    d.polygon([(9, 37), (8, 26), (12, 18), (19, 17), (23, 24), (24, 37)], fill=stone[1], outline=OUTLINE)
    d.polygon([(12, 20), (17, 19), (19, 27), (13, 30)], fill=stone[2])
    d.line([(10, 30), (11, 36)], fill=stone[0])
    # tail curling up the right side
    d.polygon([(23, 36), (28, 30), (28, 22), (25, 18), (24, 24), (25, 30), (21, 35)], fill=stone[2], outline=OUTLINE)
    d.line([(26, 21), (27, 27)], fill=stone[3])
    # head and ears
    d.polygon([(10, 17), (9, 9), (12, 4), (14, 9), (17, 9), (20, 3), (22, 9), (21, 16), (16, 19)], fill=stone[2], outline=OUTLINE)
    d.polygon([(10, 13), (5, 15), (10, 17)], fill=stone[2], outline=OUTLINE)       # snout to the left
    d.point([(5, 15)], fill=OUTLINE)
    d.point([(12, 11), (18, 11)], fill=OUTLINE)                                      # eyes
    d.line([(11, 6), (12, 8)], fill=stone[3])
    d.line([(19, 5), (20, 8)], fill=stone[3])
    # the red bib
    d.polygon([(10, 18), (21, 17), (19, 24), (15, 26), (12, 23)], fill=(176, 40, 34, 255), outline=(96, 22, 20, 255))
    d.line([(12, 19), (19, 18)], fill=(214, 72, 56, 255))
    return img


def bamboo_shoot():
    """A takenoko: a stubby cone of overlapping brown sheaths with pale tips, the earth heaped around its foot."""
    w, h = 18, 22
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([1, 16, 16, 21], fill=(96, 70, 44, 255), outline=OUTLINE)                  # heaped earth
    d.polygon([(4, 18), (6, 8), (9, 2), (12, 8), (14, 18)], fill=(132, 92, 50, 255), outline=OUTLINE)
    for k, y in enumerate((15, 11, 7)):
        d.line([(5 + k, y + 2), (9, y - 1), (13 - k, y + 2)], fill=(84, 56, 32, 255))
        d.line([(6 + k, y + 1), (8, y)], fill=(176, 132, 76, 255))
    d.polygon([(8, 4), (9, 1), (10, 4)], fill=(196, 206, 120, 255))                     # the green-gold tip
    d.point([(7, 13), (11, 9)], fill=(206, 168, 104, 255))
    return img


def ley_rift(glow_only=False):
    """The way into the leyline, lying flat (north is up in the image): split earth with roots across it and rough
    stone steps going down into the dark; cyan veins run out of it through the ground. glow_only: just the veins and
    the light along the steps' edges, on transparency (drawn emissive over the rift)."""
    w, h = 112, 80
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rnd = random.Random(71)
    cx, cy = 56, 40

    def ring(rx, ry, wobble, n=28, seed=0):
        r2 = random.Random(seed)
        pts = []
        for k in range(n):
            a = 2 * math.pi * k / n
            f = 1.0 + wobble * (r2.random() - 0.5)
            pts.append((cx + math.cos(a) * rx * f, cy + math.sin(a) * ry * f))
        return pts

    glow = (110, 236, 255, 255)
    glow_soft = (60, 170, 210, 200)
    veins = []
    for k in range(9):
        a = 2 * math.pi * (k + 0.3 * rnd.random()) / 9
        x, y = cx + math.cos(a) * 30, cy + math.sin(a) * 20
        path = [(x, y)]
        for s in range(5):
            a += (rnd.random() - 0.5) * 0.9
            x += math.cos(a) * 5
            y += math.sin(a) * 3.4
            path.append((x, y))
        veins.append(path)
    if glow_only:
        for path in veins:
            d.line(path, fill=glow_soft, width=2)
            d.line(path, fill=glow, width=1)
        # the steps' lit edges, deeper ones fainter
        for k, (rx, ry) in enumerate(((26, 16), (21, 12.5), (16, 9.5), (11, 6.5))):
            pts = ring(rx, ry, 0.12, seed=10 + k)
            alpha = 255 - k * 50
            d.line(pts[14:] + pts[:2], fill=(110, 236, 255, alpha), width=1)
        d.ellipse([cx - 6, cy - 3, cx + 6, cy + 4], fill=(140, 245, 255, 230))
        return img
    # cracked earth around it
    d.polygon(ring(38, 26, 0.25, seed=1), fill=(86, 64, 42, 255))
    d.polygon(ring(33, 22, 0.2, seed=2), fill=(62, 46, 32, 255), outline=OUTLINE)
    # steps down: rings getting darker
    tones = [(96, 92, 84, 255), (70, 68, 64, 255), (46, 46, 48, 255), (24, 26, 32, 255)]
    for k, (rx, ry) in enumerate(((26, 16), (21, 12.5), (16, 9.5), (11, 6.5))):
        pts = ring(rx, ry, 0.12, seed=10 + k)
        d.polygon(pts, fill=tones[k], outline=OUTLINE)
        # the tread's lighter lip on the far (north) side, where it catches the light
        d.line(pts[17:24], fill=(132, 128, 116, 255) if k < 2 else (70, 72, 78, 255), width=1)
    d.ellipse([cx - 7, cy - 4, cx + 7, cy + 5], fill=(8, 10, 16, 255))
    # roots across the rim
    for (a0, a1, r) in ((0.4, 1.3, 30), (2.5, 3.3, 31), (4.1, 4.9, 29)):
        pts = [(cx + math.cos(a) * r * (1 + 0.08 * math.sin(7 * a)), cy + math.sin(a) * r * 0.68) for a in
               [a0 + (a1 - a0) * t / 8 for t in range(9)]]
        d.line(pts, fill=OUTLINE, width=4)
        d.line(pts, fill=(118, 86, 52, 255), width=2)
    for path in veins:
        d.line(path, fill=(40, 70, 80, 255), width=1)                                    # dull when unlit
    # loose stones
    for _ in range(14):
        a = rnd.random() * 2 * math.pi
        r = 34 + rnd.random() * 8
        x, y = cx + math.cos(a) * r, cy + math.sin(a) * r * 0.68
        d.ellipse([x - 2, y - 1.5, x + 2, y + 1.5], fill=(120, 116, 106, 255), outline=OUTLINE)
    return img


def save(img, folder, name):
    path = os.path.join(folder, name + ".png")
    img.save(path)
    meta(path, name)


def main():
    trees = os.path.join(ASSETS, "Sprites", "Trees")
    props = os.path.join(ASSETS, "Sprites", "Props")
    for name, size, stalks, seed in (("BambooA", (72, 300), 6, 11), ("BambooB", (60, 270), 4, 23), ("BambooC", (88, 320), 8, 37)):
        path = os.path.join(trees, name + ".png")
        bamboo(size[0], size[1], stalks, seed).save(path)
        meta(path, name)
    path = os.path.join(props, "CampTarp.png")
    camp_tarp().save(path)
    meta(path, "CampTarp")
    save(kitsune_statue(), props, "KitsuneStatue")
    save(bamboo_shoot(), props, "BambooShoot")
    save(ley_rift(), props, "LeyRift")
    save(ley_rift(glow_only=True), props, "LeyRiftGlow")
    print("wrote BambooA-C, CampTarp, KitsuneStatue, BambooShoot, LeyRift and LeyRiftGlow")


if __name__ == "__main__":
    main()
