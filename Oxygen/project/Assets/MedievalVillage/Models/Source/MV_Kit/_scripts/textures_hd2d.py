"""EONLORE Medieval Village kit - HD-2D texture pass (Oct 30, 2026).

Pushes the kit textures towards the Octopath Traveler II look: chunky, bevelled shapes (every stone, shingle and board
gets a lit upper-left rim, a shaded lower-right rim and a dark joint), bigger value range inside each element, warm
highlights against cool shadows, moss and weathering in clusters, and 4 m tiles (128 px) so repeats stop showing.

Rules kept from textures.py: 32 texels per metre, seamless tiling, image-down = ground / downhill, few-step ramps,
no dithering, lit from the upper left. Output names are the kit's existing ones (materials keep working) plus
MV_Wood_Shutter.png (painted shutters) and MV_Cobble_Road.png (small road cobbles, 6-9 px).

Run: python textures_hd2d.py <outdir>      (numpy + Pillow; writes _contact_hd2d.png for review)
"""
import os
import sys

import numpy as np
from PIL import Image

PPM = 32


def hexramp(*hs):
    return np.array([[int(h[i:i + 2], 16) for i in (1, 3, 5)] for h in hs], dtype=np.uint8)


# ---------------------------------------------------------------- ramps (dark -> light, cool shadows, warm lights)
STONE = hexramp('#24201f', '#37312d', '#4d463f', '#665e53', '#80776a', '#9b9180', '#b6ab95', '#cfc4aa')
STONE_COOL = hexramp('#22262b', '#323940', '#465057', '#5d676c', '#778084', '#939a99', '#afb3ac')
ASHLAR = hexramp('#30271f', '#463a2f', '#5f5142', '#7a6a56', '#95846b', '#ae9d80', '#c6b597', '#dccdb0')
PLASTER = hexramp('#6f5e4c', '#8c7861', '#a69177', '#bba78a', '#cebb9c', '#ddccad', '#e9dbbf')
TIMBER = hexramp('#1a110b', '#28190f', '#3a2415', '#4e311c', '#634024', '#7a512e', '#91633a')
DOORWOOD = hexramp('#22160e', '#33210f', '#4a2f18', '#5f3d1f', '#744c27', '#8a5c30', '#a06e3c')
PLANK = hexramp('#211a16', '#30271f', '#43372b', '#574937', '#6c5c45', '#837155', '#9b8767', '#b29e7c')
SLATE_BLUE = hexramp('#161c27', '#1f2a3a', '#29394f', '#344a64', '#425e79', '#56778f', '#7092a3', '#93b0b8')
SHINGLE = hexramp('#1e1510', '#2c1f17', '#3d2b1f', '#513a29', '#664a34', '#7c5c40', '#93704f', '#a98662')
THATCH = hexramp('#271b0f', '#3c2b16', '#55401f', '#6f5529', '#886a33', '#9f7f3f', '#b4934e', '#c6a662')
SHUTTER = hexramp('#14211c', '#1c3428', '#264934', '#335f40', '#467650', '#5f8c5c', '#7fa46c')
MOSS = hexramp('#1d2716', '#29361d', '#384a24', '#4a5d2b', '#617233', '#7c873f')
LICHEN = hexramp('#6f7550', '#8a8d5e', '#a3a46c')
IRON = hexramp('#15161a', '#22242a', '#33363e', '#4a4e57', '#666b74')
DIRT = hexramp('#33251a', '#463324', '#5a422e', '#6e5239', '#836345', '#987553', '#ad8964')
GLOW = hexramp('#7a3e14', '#c97a2a', '#f2a43a', '#ffd27a', '#fff0c0')
CURTAIN = hexramp('#5a2620', '#7a352a', '#984a36')


# ---------------------------------------------------------------- noise (all tileable)
def value_noise(w, h, cell, seed):
    r = np.random.default_rng(seed)
    gw, gh = max(1, w // cell), max(1, h // cell)
    g = r.random((gh, gw))
    ys = np.arange(h) / (h / gh)
    xs = np.arange(w) / (w / gw)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
    a = g[y0[:, None] % gh, x0[None, :] % gw]
    b = g[y0[:, None] % gh, (x0[None, :] + 1) % gw]
    c = g[(y0[:, None] + 1) % gh, x0[None, :] % gw]
    d = g[(y0[:, None] + 1) % gh, (x0[None, :] + 1) % gw]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(w, h, cells, seed):
    acc = np.zeros((h, w)); tot = 0.0
    for i, c in enumerate(cells):
        amp = 1.0 / (i + 1)
        acc += value_noise(w, h, c, seed + i * 17) * amp; tot += amp
    return acc / tot


def paint(idx, ramp):
    return ramp[np.clip(np.round(idx), 0, len(ramp) - 1).astype(int)]


def voronoi(w, h, seeds, aniso=1.0):
    yy, xx = np.mgrid[0:h, 0:w]
    best = np.full((h, w), 1e9); lab = np.zeros((h, w), int)
    for i, (sx, sy) in enumerate(seeds):
        dx = np.abs(xx - sx); dx = np.minimum(dx, w - dx)
        dy = np.abs(yy - sy); dy = np.minimum(dy, h - dy)
        d = (dx / aniso) ** 2 + dy ** 2
        m = d < best; best[m] = d[m]; lab[m] = i
    return lab


def differs(lab, dy, dx):
    return lab != np.roll(np.roll(lab, dy, 0), dx, 1)


def bevel(lab, joint_w=1, rim=2, round_corners=True):
    """Per-pixel masks for chunky bevelled cells.
    joint: dark gap (the lower/right border of each cell, joint_w px).  lit1/lit2: first/second pixel inside the
    upper-left rim.  sh1/sh2: first/second pixel inside the lower-right rim.  corner: pixels near two borders
    (rounded off into the joint so stones read as rounded)."""
    joint = np.zeros(lab.shape, bool)
    for k in range(1, joint_w + 1):
        joint |= differs(lab, -k, 0) | differs(lab, 0, -k)
    if round_corners:
        diag = differs(lab, -1, -1) | differs(lab, 1, -1) | differs(lab, -1, 1)
        both = (differs(lab, 1, 0) | differs(lab, -1, 0)) & (differs(lab, 0, 1) | differs(lab, 0, -1))
        joint |= (diag & both)
    inner = ~joint
    lit1 = inner & (differs(lab, 1, 0) | differs(lab, 0, 1))
    lit2 = inner & ~lit1 & (differs(lab, 2, 0) | differs(lab, 0, 2))
    sh1 = inner & ~lit1 & (np.roll(joint, -1, 0) | np.roll(joint, -1, 1))
    sh2 = inner & ~lit1 & ~sh1 & ~lit2 & (np.roll(joint, -2, 0) | np.roll(joint, -2, 1))
    return joint, lit1, lit2, sh1, sh2


def shade_cells(lab, tone, ramp, seed, w, h, rim_lit=(2, 1), rim_sh=(-2, -1), speck=3, macro=True):
    joint, lit1, lit2, sh1, sh2 = bevel(lab)
    base = tone[lab].astype(float)
    # interior clusters (pits, chips): 3-4 px blobs, one step darker / lighter
    n = value_noise(w, h, speck, seed + 5)
    base += (n > 0.78) * 1.0 - (n < 0.2) * 1.0
    if macro:  # broad weathering patches across stones (2-4 m)
        big = fbm(w, h, [64, 32], seed + 9)
        base += (big > 0.66) * 1.0 - (big < 0.33) * 1.0
    base = np.where(lit1, base + rim_lit[0], base)
    base = np.where(lit2, base + rim_lit[1], base)
    base = np.where(sh1, base + rim_sh[0], base)
    base = np.where(sh2, base + rim_sh[1], base)
    base = np.where(joint, 0, base)
    return base, joint, sh1


def moss_overlay(img, joint, w, h, seed, ground=0.0, amount=0.0, in_joints=0.0):
    """Moss clumps: grow from the ground (image bottom) and along joints; lit tops, dark undersides."""
    yy = np.mgrid[0:h, 0:w][0] / h
    m = fbm(w, h, [32, 16, 8], seed)
    field = m * 0.9 + yy * ground + amount
    clump = field > 1.0
    jm = joint & (field > 1.0 - in_joints)
    tex = value_noise(w, h, 2, seed + 3)
    lvl = np.clip(((field - 1.0) * 8).astype(int) + 2 + (tex > 0.6) * 1 - (tex < 0.3) * 1, 1, 4)
    top = clump & ~np.roll(clump, 1, 0)                      # top row of each clump catches the light
    left = clump & ~np.roll(clump, 1, 1)
    rim = clump & (~np.roll(clump, -1, 0) | ~np.roll(clump, -1, 1))
    under = np.roll(clump, 1, 0) & ~clump                   # one pixel under the clump: shadow
    out = img.copy()
    out[clump] = MOSS[lvl[clump]]
    out[rim] = MOSS[1]
    out[top | left] = MOSS[5]
    out[jm & ~clump] = MOSS[1]
    out[under & ~joint] = (out[under & ~joint] * 0.72).astype(np.uint8)
    return out


# ---------------------------------------------------------------- stones
def course_seeds(w, h, row, stone_w, r, jitter=1.5):
    seeds = []
    y = r.uniform(0, row)
    while y < h + row:
        x = r.uniform(0, stone_w)
        while x < w:
            seeds.append((x + r.normal(0, jitter), (y + r.normal(0, jitter * 0.7)) % h))
            x += r.uniform(stone_w * 0.65, stone_w * 1.35)
        y += r.uniform(row * 0.85, row * 1.15)
    return np.array(seeds)


def field_stone(w=128, h=64, ramp=STONE, seed=1, moss_ground=0.75, row=10, stone_w=15, warm_mix=True):
    """Rubble / field-stone wall: rounded stones 0.3-0.6 m in rough courses, dark mortar, chunky bevels."""
    r = np.random.default_rng(seed)
    seeds = course_seeds(w, h, row, stone_w, r)
    lab = voronoi(w, h, seeds, aniso=1.45)
    tone = r.choice([2, 3, 3, 4, 4, 5], len(seeds)).astype(float)
    base, joint, _ = shade_cells(lab, tone, ramp, seed, w, h)
    img = paint(base, ramp)
    if warm_mix:  # a few ochre / rust stones
        warm = r.random(len(seeds)) < 0.14
        wm = warm[lab] & ~joint
        img[wm] = np.clip(img[wm].astype(int) + np.array([14, 6, -6]), 0, 255).astype(np.uint8)
    if moss_ground:
        img = moss_overlay(img, joint, w, h, seed + 21, ground=moss_ground, amount=-0.22, in_joints=0.3)
    return img


def ashlar(w=128, h=128, ramp=ASHLAR, seed=2, course=10, moss=True):
    """Dressed stone blocks (0.3 m courses, 0.4-0.8 m long), soft rounded corners, chipped edges, stains."""
    r = np.random.default_rng(seed)
    lab = np.zeros((h, w), int); n = 0
    heights = [course] * (h // course)
    i = 0
    while sum(heights) < h:                                   # spread the remainder: some courses 1 px taller
        heights[i % len(heights)] += 1; i += 3
    r.shuffle(heights)
    y0 = 0
    for hh in heights:
        x = -int(r.integers(0, 20))
        while x < w:
            L = int(r.choice([13, 16, 18, 20, 22, 25]))
            for xx in range(x, x + L):
                lab[y0:y0 + hh, xx % w] = n
            n += 1; x += L
        y0 += hh
    tone = r.choice([3, 3, 4, 4, 4, 5], n).astype(float)
    base, joint, _ = shade_cells(lab, tone, ramp, seed, w, h, speck=4)
    # chips: knock one rim pixel into the joint here and there
    chip = (value_noise(w, h, 2, seed + 13) > 0.86) & np.roll(joint, 1, 0)
    base = np.where(chip, 1, base)
    img = paint(base, ramp)
    # rain stains: vertical darker streaks starting under some joints
    streak = value_noise(w, 8, 2, seed + 3)[0] > 0.8
    yy, xx = np.mgrid[0:h, 0:w]
    st = streak[xx] & (fbm(w, h, [16, 8], seed + 4) > 0.55) & ~joint
    img[st] = (img[st] * 0.86).astype(np.uint8)
    if moss:
        img = moss_overlay(img, joint, w, h, seed + 7, ground=0.0, amount=-0.06, in_joints=0.12)
    return img


def cobble_road(w=128, h=128, ramp=STONE, seed=17):
    """Road cobbles 6-9 px (0.2-0.3 m), rounded, worn smooth tops, dirt in the joints."""
    r = np.random.default_rng(seed)
    seeds = course_seeds(w, h, 7, 8, r, jitter=1.1)
    lab = voronoi(w, h, seeds, aniso=1.15)
    tone = r.choice([2, 3, 3, 4, 4, 5], len(seeds)).astype(float)
    base, joint, _ = shade_cells(lab, tone, ramp, seed, w, h, rim_lit=(2, 1), rim_sh=(-1, -1), speck=3)
    img = paint(base, ramp)
    dj = joint & (fbm(w, h, [16, 8], seed + 3) > 0.45)
    img[dj] = DIRT[1]
    grass = joint & (fbm(w, h, [32, 8], seed + 8) > 0.74)
    img[grass] = MOSS[2]
    return img


# ---------------------------------------------------------------- plaster
def plaster(w=128, h=128, seed=3):
    """Lime plaster: soft broad mottling (2-3 steps), a few patches where it fell off and shows rubble stone,
    hairline cracks. Image bottom is the foot of the wall: a little splash dirt there."""
    r = np.random.default_rng(seed)
    big = fbm(w, h, [64, 32, 16], seed)
    blot = value_noise(w, h, 4, seed + 31)
    fine = value_noise(w, h, 3, seed + 33)
    idx = 3.0 + (big - 0.5) * 2.6 + (blot > 0.84) * 1.0 - (blot < 0.12) * 1.0 + (fine > 0.8) * 1.0
    idx = np.clip(np.round(idx), 1, 6)
    img = paint(idx, PLASTER)
    yy, xx = np.mgrid[0:h, 0:w]
    # fallen-off patches showing stone (2-3 per tile), with a lit lower lip and a dark upper lip (plaster is thick)
    for k, (cx, cy) in enumerate(((int(r.integers(0, 50)), int(r.integers(14, 50))), (int(r.integers(64, 120)), int(r.integers(70, 104))))):
        rx, ry = int(r.integers(12, 17)), int(r.integers(7, 10))
        dx = np.minimum(np.abs(xx - cx), w - np.abs(xx - cx)); dy = np.abs(yy - cy)
        wob = fbm(w, h, [8, 4], seed + 40 + k)
        hole = ((dx / rx) ** 2 + (dy / ry) ** 2) < 0.75 + wob * 0.5
        stones = field_stone(w, h, STONE, seed=60 + k, moss_ground=0, row=7, stone_w=10, warm_mix=False)
        img[hole] = stones[hole]
        upper = np.roll(hole, -1, 0) & ~hole        # plaster edge above the hole: shadow inside the hole
        lower = np.roll(hole, 1, 0) & ~hole         # plaster edge below: catches light
        img[np.roll(upper, 1, 0) & hole] = STONE[1]
        img[upper] = PLASTER[1]
        img[lower] = PLASTER[6]
    # hairline cracks (dark with a lit pixel to the left)
    for _ in range(5):
        x, y = int(r.integers(0, w)), int(r.integers(0, h))
        for _ in range(int(r.integers(5, 12))):
            img[y % h, x % w] = PLASTER[0]
            img[y % h, (x - 1) % w] = PLASTER[5]
            y += 1; x += int(r.choice([-1, 0, 0, 1]))
    return img


# ---------------------------------------------------------------- wood
def beam_wood(w=16, h=128, ramp=TIMBER, seed=4):
    """Adzed timber, grain along V: lit left edge, shaded right edge, long grain streaks, knots, checks (cracks)."""
    r = np.random.default_rng(seed)
    idx = np.full((h, w), 3.0)
    streak = value_noise(w, h, 8, seed)
    idx += (streak > 0.68) * 1.0 - (streak < 0.28) * 1.0
    for _ in range(10):
        x = int(r.integers(1, w - 1)); y0 = int(r.integers(0, h)); L = int(r.integers(8, 30))
        idx[np.arange(y0, y0 + L) % h, x] -= 1
    for _ in range(3):  # checks: a dark 1 px crack with a lit pixel beside it
        x = int(r.integers(3, w - 3)); y0 = int(r.integers(0, h)); L = int(r.integers(10, 26))
        ys = np.arange(y0, y0 + L) % h
        idx[ys, x] = 0; idx[ys, x - 1] += 1
    for _ in range(3):  # knots
        x, y = int(r.integers(3, w - 3)), int(r.integers(0, h))
        idx[(y - 1) % h, x - 1:x + 2] += 1; idx[y % h, x - 1:x + 2] = 1; idx[y % h, x] = 0; idx[(y + 1) % h, x - 1:x + 2] -= 1
    # bevel across the width: lit left 2 px, dark right 2 px (beams are 0.15-0.2 m, the texture is 0.5 m wide)
    idx[:, 0:2] += 1; idx[:, 1] += 1; idx[:, -2:] -= 1; idx[:, -1] -= 1
    return paint(idx, ramp)


def boards(w, h, ramp, seed, bw=(6, 7, 8, 9), knots=4, nails=True, weather=True, base_tones=(3, 3, 4, 4, 5)):
    r = np.random.default_rng(seed)
    idx = np.zeros((h, w))
    x = 0
    while x < w:
        pw = int(r.choice(bw)); pw = w - x if w - x < pw + 4 else pw
        t = float(r.choice(base_tones))
        g = value_noise(pw, h, 8, seed + x)
        board = t + (g > 0.7) * 1.0 - (g < 0.25) * 1.0
        # grain lines
        for _ in range(3):
            gx = int(r.integers(1, max(2, pw - 1))); gy = int(r.integers(0, h)); L = int(r.integers(10, 40))
            board[np.arange(gy, gy + L) % h, gx] -= 1
        idx[:, x:x + pw] = board
        idx[:, x] = 0                                  # gap
        idx[:, (x + 1) % w] += 1.5                     # lit left edge
        idx[:, (x + pw - 1) % w] -= 1                  # shaded right edge
        if r.random() < 0.6:                           # butt joint (two boards in one column)
            y = int(r.integers(4, h - 4))
            idx[y, x:x + pw] = 0; idx[(y + 1) % h, x + 1:x + pw] += 1
        if nails:
            for ny in (int(r.integers(2, 8)), int(r.integers(h // 2 - 4, h // 2 + 4))):
                if pw > 4:
                    idx[ny, x + 2] = 0; idx[ny, x + pw - 3] = 0
                    idx[(ny - 1) % h, x + 2] += 1
        for _ in range(int(r.integers(0, knots + 1))):
            ky = int(r.integers(0, h)); kx = x + int(r.integers(2, max(3, pw - 2)))
            idx[ky % h, kx % w] = 1; idx[(ky - 1) % h, kx % w] += 1
        x += pw
    img = paint(idx, ramp)
    if weather:  # silvered, sun-bleached patches high up, damp dark band at the bottom
        yy = np.mgrid[0:h, 0:w][0] / h
        bleach = fbm(w, h, [32, 16], seed + 50) > 0.7
        img[bleach] = np.clip(img[bleach].astype(int) + np.array([10, 10, 12]), 0, 255).astype(np.uint8)
        damp = (fbm(w, h, [16, 8], seed + 51) * 0.5 + yy * 0.7) > 0.86
        img[damp] = (img[damp] * 0.8).astype(np.uint8)
    return img


def planks(w=128, h=128, seed=13):
    return boards(w, h, PLANK, seed)


def shutter(w=32, h=64, seed=19):
    """Painted plank shutter / door leaf (1 x 2 m, fills a face): faded green paint, bare wood where it wore off."""
    img = boards(w, h, SHUTTER, seed, bw=(6, 7, 8), knots=2, weather=False, base_tones=(3, 3, 4))
    wear = (fbm(w, h, [8, 4], seed + 4) > 0.66)
    bare = boards(w, h, PLANK, seed, bw=(6, 7, 8), knots=0, weather=False)
    img[wear] = bare[wear]
    # z-brace and two ledges in a darker green
    for y0 in (8, h - 12):
        img[y0:y0 + 4, 1:w - 1] = SHUTTER[3]; img[y0, 1:w - 1] = SHUTTER[5]; img[y0 + 4, 1:w - 1] = SHUTTER[0]
    img[:, 0] = SHUTTER[0]; img[0, :] = SHUTTER[0]
    return img


def door(w=32, h=64, seed=5):
    """Plank door 1 x 2 m: five boards, two iron straps with nail heads, ring handle, dark frame edge, worn threshold."""
    img = boards(w, h, DOORWOOD, seed, bw=(6, 6, 7), knots=2, nails=False, weather=False, base_tones=(3, 3, 4))
    for sy in (10, 48):
        img[sy:sy + 3, 2:w - 2] = IRON[2]
        img[sy, 2:w - 2] = IRON[4]
        img[sy + 3, 2:w - 2] = DOORWOOD[0]
        for nx in range(4, w - 3, 5):
            img[sy + 1, nx] = IRON[4]
        img[sy + 1:sy + 3, w - 4:w - 2] = IRON[3]                   # strap end
    hx, hy = w - 8, 32
    for (dx, dy) in [(0, 0), (1, 0), (-1, 1), (2, 1), (-1, 2), (2, 2), (0, 3), (1, 3)]:
        img[hy + dy, hx + dx] = IRON[4] if dy < 2 else IRON[1]
    img[hy - 1, hx:hx + 2] = IRON[3]
    img[:, 0] = DOORWOOD[0]; img[:, -1] = DOORWOOD[0]; img[0, :] = DOORWOOD[0]
    img[h - 3:, 1:-1] = DOORWOOD[2]; img[h - 1, :] = DOORWOOD[0]
    return img


# ---------------------------------------------------------------- roofs (rows along U, image-down = downhill)
def roof_tiles(w, h, ramp, row, minw, maxw, seed, scallop=True, moss=0.0, lichen=0.0, repair=0.06):
    """Each shingle/slate: the row above covers its top (dark band), its own face brightens down the slope, the exposed
    lower lip catches the light (2 px), and it throws a 1-2 px shadow onto the row below. Rounded lower corners
    (scallop) for slate; square, ragged lengths for wood shingles. A few replaced (lighter) tiles = repairs."""
    r = np.random.default_rng(seed)
    lab = -np.ones((h, w), int); tone = []; n = 0
    rows = h // row
    for ry in range(rows):
        x = int(r.integers(0, maxw)) - maxw
        y0 = ry * row
        while x < w:
            L = int(r.integers(minw, maxw + 1))
            hang = int(r.integers(0, 2))                  # some tiles hang 1 px lower: ragged row edge
            for k, xx in enumerate(range(x, x + L)):
                X = xx % w
                bottom = row + hang
                if scallop and (k == 0 or k == L - 1):
                    bottom -= 2                             # rounded corners
                elif scallop and (k == 1 or k == L - 2):
                    bottom -= 1
                for yy in range(bottom):
                    lab[(y0 + yy) % h, X] = n
            t = float(r.choice([2, 3, 3, 4, 4, 5]))
            if r.random() < repair:
                t += 1.5
            tone.append(t); n += 1; x += L
    tone = np.array(tone)
    yy = np.mgrid[0:h, 0:w][0]
    # local y inside the tile (0 = top, covered)
    top = np.zeros((h, w));
    for ry in range(rows):
        top[ry * row:(ry + 1) * row] = np.arange(row)[:, None]
    # position along the tile's exposed height: brighten towards the lip
    face = tone[lab] + np.floor(top / row * 2.2) - 1.0
    gap = differs(lab, 0, -1)                               # vertical gap at the right of each tile
    lip = (lab != np.roll(lab, -1, 0))                      # last pixel of a tile before the row below
    lip2 = np.roll(lip, 1, 0) & ~lip
    shadow = np.roll(lip, 1, 0)                             # first pixel of the row below the lip
    shadow2 = np.roll(lip, 2, 0)
    idx = face.copy()
    idx = np.where(lip2, idx + 1, idx)
    idx = np.where(lip, idx + 2, idx)
    leftlit = differs(lab, 0, 1) & ~gap
    idx = np.where(leftlit, idx + 1, idx)
    idx = np.where(shadow, 0, idx)
    idx = np.where(shadow2 & ~lip, idx - 2, idx)
    idx = np.where(gap, 0, idx)
    big = fbm(w, h, [64, 32], seed + 2)
    idx += (big > 0.68) * 1.0 - (big < 0.3) * 1.0          # broad sun-faded and dark-weathered patches
    img = paint(idx, ramp)
    joint = gap | shadow
    if lichen:
        lm = (value_noise(w, h, 3, seed + 31) > 1.0 - lichen) & ~joint
        img[lm] = LICHEN[np.clip((value_noise(w, h, 2, seed + 32)[lm] * 3).astype(int), 0, 2)]
    if moss:
        img = moss_overlay(img, joint, w, h, seed + 11, ground=0.0, amount=moss - 0.3, in_joints=0.25)
    return img


def slate_blue(w=128, h=96):
    return roof_tiles(w, h, SLATE_BLUE, row=8, minw=6, maxw=9, seed=6, scallop=True, moss=0.14, lichen=0.03)


def shingle_wood(w=128, h=128):
    return roof_tiles(w, h, SHINGLE, row=8, minw=3, maxw=6, seed=12, scallop=False, moss=0.2, lichen=0.02, repair=0.08)


def thatch(w=128, h=128, seed=21):
    """Thatch in 0.5 m bundle courses: each course is a rounded roll (dark under the course above, bright belly, dark
    combed lower edge with straw tips hanging over), vertical straw strands, moss and darker rotten patches."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]
    band = 16
    b = yy // band
    # ragged lower edge per 2 px column
    edge = np.repeat(r.integers(-2, 3, (h // band, w // 2)), 2, axis=1)[b % (h // band), xx]
    ph = (yy % band) - edge
    prof = np.select([ph < 2, ph < 4, ph < 7, ph < 11, ph < 14], [0, 2, 4, 5, 4], 3).astype(float)
    # strands: columns of +-1 that run along a course
    strand = np.repeat(r.integers(-1, 2, (h // band, w // 2)), 2, axis=1)[b % (h // band), xx]
    idx = prof + np.where(prof >= 2, strand, 0)
    tips = (ph >= 14) & (r.random((h, w)) < 0.35)
    idx = np.where(tips, 1, idx)
    big = fbm(w, h, [64, 32, 16], seed)
    idx += (big > 0.68) * 1.0 - (big < 0.3) * 1.0
    img = paint(idx, THATCH)
    joint = prof < 1
    img = moss_overlay(img, joint, w, h, seed + 9, ground=0.0, amount=-0.12, in_joints=0.2)
    return img


# ---------------------------------------------------------------- ground
def dirt_road(w=128, h=128, seed=31):
    """Packed dirt: quiet broad base, lit pebbles with a shadow pixel, darker wheel ruts, a few grass tufts."""
    yy, xx = np.mgrid[0:h, 0:w]
    n = fbm(w, h, [64, 32, 16], seed)
    base = 3.0 + (n > 0.68) * 1.0 - (n < 0.3) * 1.0
    clods = value_noise(w, h, 3, seed + 3) > 0.86
    base = np.where(clods, base + 1, base)
    r = np.random.default_rng(seed)
    img = paint(base, DIRT)
    ruts = (np.abs((yy % 64) - 18) < 2) & (value_noise(w, h, 8, seed + 11) > 0.55)
    img[ruts] = (img[ruts] * 0.84).astype(np.uint8)
    for _ in range(70):   # pebbles 1-3 px: a lit top-left, a shadow under
        px, py = int(r.integers(0, w)), int(r.integers(0, h))
        s = int(r.choice([1, 1, 2, 2, 3]))
        tone = int(r.integers(3, 6))
        for dy in range(s):
            for dx in range(s):
                img[(py + dy) % h, (px + dx) % w] = STONE[tone]
        img[py % h, px % w] = STONE[min(tone + 2, 7)]
        img[(py + s) % h, px % w:px % w + s] = DIRT[0]
    t = value_noise(w, h, 4, seed + 5) > 0.92
    img[t] = MOSS[3]
    img[t & ~np.roll(t, 1, 0)] = MOSS[5]
    return img


def tilled(w=128, h=128, seed=14):
    """Tilled soil: furrows along U every 16 px (0.5 m): lit ridge, dark trough, clods, weeds."""
    yy, xx = np.mgrid[0:h, 0:w]
    phase = (yy + (value_noise(w, h, 16, seed + 1) * 2).astype(int)) % 16
    base = np.select([phase < 2, phase < 5, phase < 10, phase < 13], [1, 3, 5, 4], 2).astype(float)
    n = fbm(w, h, [32, 16], seed)
    base += (n > 0.68) * 1.0 - (n < 0.3) * 1.0
    clods = value_noise(w, h, 2, seed + 3) > 0.86
    base = np.where(clods & (phase > 3), base + 1, base)
    img = paint(base, DIRT)
    m = value_noise(w, h, 3, seed + 5) > 0.92
    img[m & (phase > 4) & (phase < 10)] = MOSS[3]
    return img


def water(w=64, h=64, seed=15):
    WATER = hexramp('#0d2b2e', '#123A3C', '#185052', '#1E6565', '#3f8f86', '#7BC9B8')
    n = fbm(w, h, [32, 16], seed)
    idx = 1 + (n > 0.45) * 1 + (n > 0.62) * 1
    yy, xx = np.mgrid[0:h, 0:w]
    rip = (value_noise(w, h, 4, seed + 2) > 0.78) & ((yy % 6) == 0)
    idx = np.where(rip, 4, idx)
    sp = (value_noise(w, h, 2, seed + 4) > 0.93) & ((yy % 6) == 1)
    idx = np.where(sp, 5, idx)
    return paint(idx, WATER)


# ---------------------------------------------------------------- small pieces
def iron(w=16, h=16, seed=8):
    n = value_noise(w, h, 4, seed)
    idx = 2 + (n > 0.65) * 1 - (n < 0.3) * 1
    img = paint(idx, IRON)
    img[0, :] = IRON[3]; img[:, 0] = IRON[3]
    return img


def cloth(w=32, h=32, seed=9):
    RED = hexramp('#4a1c18', '#6e2a22', '#8c3a2c', '#a64e38', '#bd6446')
    CREAM = hexramp('#5b4a37', '#75604a', '#8f785c', '#a8916f', '#bea684', '#d2bc98')
    yy, xx = np.mgrid[0:h, 0:w]
    stripe = (xx // 8) % 2 == 1
    fold = (xx % 8) == 7
    lit = (xx % 8) == 0
    n = fbm(w, h, [16, 8], seed)
    idx = 3 + (n > 0.62) * 1 - (n < 0.32) * 1 - fold * 2 + lit * 1
    return np.where(stripe[..., None], RED[np.clip(idx - 1, 0, 4)], CREAM[np.clip(idx, 0, 5)])


def window(w=32, h=32):
    """Leaded glazing with a warm lit room behind: curtain edges, stepped glow, lead cames, wooden mullion cross."""
    yy, xx = np.mgrid[0:h, 0:w]
    cx, cy = w / 2, h * 0.62
    d = np.sqrt(((xx - cx) / w) ** 2 + ((yy - cy) / h) ** 2)
    step = np.clip((0.66 - d) * 6.5, 0, 4).astype(int)
    alb = GLOW[step].copy()
    curtain = (xx < 4) | (xx > w - 5)
    alb[curtain] = CURTAIN[np.clip((yy[curtain] // 6) % 3, 0, 2)]
    lead = (xx % 8 == 0) | (yy % 8 == 0) | (xx == w - 1) | (yy == h - 1)
    alb[lead] = IRON[1]
    alb[:, w // 2 - 1:w // 2 + 1] = TIMBER[2]
    alb[h // 2 - 1:h // 2 + 1, :] = TIMBER[2]
    alb[h // 2 - 2, :] = TIMBER[4]
    emi = alb.copy()
    emi[lead | curtain] = 0
    emi[curtain] = (CURTAIN[2] * 0.6).astype(np.uint8)
    emi[:, w // 2 - 1:w // 2 + 1] = 0; emi[h // 2 - 2:h // 2 + 1, :] = 0
    return alb, emi


def sign(w=32, h=24):
    img = door(w, h * 3)[24:24 + h].copy()
    img[0:2, :] = TIMBER[1]; img[-2:, :] = TIMBER[1]; img[:, 0:2] = TIMBER[1]; img[:, -2:] = TIMBER[1]
    img[1, 1:-1] = TIMBER[5]
    paint_c = PLASTER[6]; sh = PLASTER[2]
    for y in range(7, 18):
        img[y, 11:19] = paint_c
        img[y, 18] = sh
    img[6, 10:20] = PLASTER[5]
    img[5, 11:15] = PLASTER[6]; img[5, 16:18] = PLASTER[6]
    for y in (9, 15):
        img[y, 11:19] = sh
    for y in range(9, 16):
        img[y, 20] = paint_c
    img[9, 19] = paint_c; img[15, 19] = paint_c
    return img


# ---------------------------------------------------------------- main
def build():
    tex = {
        'MV_Stone_Field.png': field_stone(),                                                    # 4 x 2 m
        'MV_Stone_Cut.png': ashlar(),                                                           # 4 x 4 m
        'MV_Stone_Chimney.png': field_stone(128, 128, STONE_COOL, seed=33, moss_ground=0, row=9, stone_w=13),
        'MV_Plaster_Warm.png': plaster(),                                                       # 4 x 4 m
        'MV_Wood_DarkBeam.png': beam_wood(),                                                    # 0.5 x 4 m
        'MV_Wood_Door.png': door(),                                                             # 1 x 2 m (fit)
        'MV_Wood_Shutter.png': shutter(),                                                       # 1 x 2 m (fit)
        'MV_Wood_Planks.png': planks(),                                                         # 4 x 4 m
        'MV_Roof_SlateDark.png': slate_blue(),                                                  # 4 x 3 m
        'MV_Roof_ShingleDark.png': shingle_wood(),                                              # 4 x 4 m
        'MV_Roof_Thatch.png': thatch(),                                                         # 4 x 4 m
        'MV_Dirt_Road.png': dirt_road(),                                                        # 4 x 4 m
        'MV_Cobble_Road.png': cobble_road(),                                                    # 4 x 4 m
        'MV_Soil_Tilled.png': tilled(),                                                         # 4 x 4 m
        'MV_Water_Stream.png': water(),                                                         # 2 x 2 m
        'MV_Metal_IronDark.png': iron(),
        'MV_Cloth_WarmNeutral.png': cloth(),
        'MV_Sign_Tavern.png': sign(),
    }
    a, e = window()
    tex['MV_Window_Glass.png'] = a
    tex['MV_Window_Glass_Emissive.png'] = e
    return tex


def contact(tex, path, S=3, tile=2):
    """Review sheet: every texture tiled 2 x 2 (to show seams) at S x."""
    items = list(tex.items())
    W = 1700
    pos = []; x = y = rowh = 0
    for k, v in items:
        t = np.tile(v, (tile, tile, 1)) if not k.startswith(('MV_Wood_Door', 'MV_Wood_Shutter', 'MV_Window', 'MV_Sign')) else v
        im = Image.fromarray(t.astype(np.uint8)).resize((t.shape[1] * S, t.shape[0] * S), Image.NEAREST)
        if x + im.width > W:
            x = 0; y += rowh + 12; rowh = 0
        pos.append((im, x, y)); x += im.width + 12; rowh = max(rowh, im.height)
    sheet = Image.new('RGB', (W, y + rowh), (34, 30, 40))
    for im, x, y in pos:
        sheet.paste(im, (x, y))
    sheet.save(path)


def main(out):
    os.makedirs(out, exist_ok=True)
    tex = build()
    for k, v in tex.items():
        Image.fromarray(v.astype(np.uint8), 'RGB').save(os.path.join(out, k))
    contact(tex, os.path.join(out, '_contact_hd2d.png'))
    for k, v in tex.items():
        print('%-32s %4d x %-4d' % (k, v.shape[1], v.shape[0]))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'out_hd2d')
