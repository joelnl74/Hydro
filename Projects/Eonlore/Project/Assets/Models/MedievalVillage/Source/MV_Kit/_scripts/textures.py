"""EONLORE Medieval Village kit - pixel-art material textures.

All textures are authored at 32 px per metre (the Shrine Glade texel density),
tile seamlessly, use few-step ramps from the Hydro style guide and are lit
from the upper left. Run: python textures.py <outdir>
"""
import sys, os
import numpy as np
from PIL import Image

PPM = 32
rng = np.random.default_rng(7)


def hexramp(*hs):
    return np.array([[int(h[i:i + 2], 16) for i in (1, 3, 5)] for h in hs], dtype=np.uint8)


# Ramps (dark -> light). Stone / plaster / timber / moss from Style-Guide.md;
# slate + shingle are new, tuned against the shrine roof (charcoal blue-grey).
STONE = hexramp('#2c2a27', '#3e3b36', '#5a564e', '#7a756a', '#9a9384', '#b9b19f')
STONE_COOL = hexramp('#2a2c2e', '#3b3e42', '#53575b', '#6d7173', '#8a8c8a', '#a7a79f')
MOSS = hexramp('#24331b', '#2b3d1e', '#3f5527', '#5a7030', '#7d8c3a')
PLASTER = hexramp('#8a7a62', '#a39177', '#b9a687', '#cbb898', '#d8c6a4', '#e2d2b0')
BRICK = hexramp('#5a3a2c', '#7a4e38', '#93624a')
TIMBER = hexramp('#1f140c', '#2a1a10', '#3B2317', '#4a2c1b', '#5a3820', '#6e4526')
DOORWOOD = hexramp('#24170e', '#33210f', '#4a2f18', '#5f3d1f', '#744c27', '#8a5c30')
SLATE = hexramp('#121419', '#1b1e26', '#252a35', '#303644', '#3d4455', '#4d5568', '#606a7e')
SHINGLE = hexramp('#17110d', '#211812', '#2d2119', '#3a2b20', '#4a382a', '#5c4634')
IRON = hexramp('#15161a', '#22242a', '#33363e', '#4a4e57', '#666b74')
CLOTH = hexramp('#5b4a37', '#75604a', '#8f785c', '#a8916f', '#bea684')
GLOW = hexramp('#7a3e14', '#c97a2a', '#f2a43a', '#ffd27a', '#fff0c0')


def value_noise(w, h, cell, seed):
    """Tileable value noise in [0,1] (bilinear over a wrapped lattice)."""
    r = np.random.default_rng(seed)
    gw, gh = max(1, w // cell), max(1, h // cell)
    g = r.random((gh, gw))
    ys = np.arange(h) / cell
    xs = np.arange(w) / cell
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
    a = g[y0[:, None] % gh, x0[None, :] % gw]
    b = g[y0[:, None] % gh, (x0[None, :] + 1) % gw]
    c = g[(y0[:, None] + 1) % gh, x0[None, :] % gw]
    d = g[(y0[:, None] + 1) % gh, (x0[None, :] + 1) % gw]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(w, h, cells, seed):
    acc = np.zeros((h, w)); tot = 0
    for i, c in enumerate(cells):
        amp = 1.0 / (i + 1)
        acc += value_noise(w, h, c, seed + i * 17) * amp; tot += amp
    return acc / tot


def cells_voronoi(w, h, seeds, aniso=1.0):
    """Wrapped Voronoi labels. seeds: (n,2) array of (x,y). aniso >1 stretches cells horizontally."""
    yy, xx = np.mgrid[0:h, 0:w]
    best = np.full((h, w), 1e9); lab = np.zeros((h, w), int)
    for i, (sx, sy) in enumerate(seeds):
        dx = np.abs(xx - sx); dx = np.minimum(dx, w - dx)
        dy = np.abs(yy - sy); dy = np.minimum(dy, h - dy)
        d = (dx / aniso) ** 2 + dy ** 2
        m = d < best; best[m] = d[m]; lab[m] = i
    return lab


def edges(lab):
    """Masks: joint (border pixel), lit (upper-left inner edge), shade (lower-right inner edge)."""
    up = np.roll(lab, 1, 0); left = np.roll(lab, 1, 1)
    down = np.roll(lab, -1, 0); right = np.roll(lab, -1, 1)
    joint = (lab != down) | (lab != right)
    lit = ((lab != up) | (lab != left)) & ~joint
    up2 = np.roll(lab, 2, 0)
    shade = (np.roll(joint, 1, 0) | np.roll(joint, 1, 1)) & ~joint
    return joint, lit, shade, up2


def paint(idx, ramp):
    idx = np.clip(idx, 0, len(ramp) - 1).astype(int)
    return ramp[idx]


def bayer4():
    return np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def dither_tile(w, h):
    b = bayer4()
    return np.tile(b, (h // 4 + 1, w // 4 + 1))[:h, :w]


# ---------------------------------------------------------------- stone field
def stone_field(w=128, h=64, moss_bottom=True, ramp=STONE, seed=1):
    r = np.random.default_rng(seed)
    # course-biased seeds: rows ~ every 9 px, stones ~ 14 px wide
    seeds = []
    y = 0.0
    while y < h - 2:
        x = r.random() * 10
        while x < w - 1:
            seeds.append((x + r.normal(0, 1.5), (y + r.normal(0, 1.2)) % h))
            x += r.uniform(10, 18)
        y += r.uniform(8, 11)
    seeds = np.array(seeds)
    lab = cells_voronoi(w, h, seeds, aniso=1.55)
    joint, lit, shade, _ = edges(lab)
    tone = r.choice([1, 2, 2, 3, 3, 4], len(seeds))  # per-stone base step
    base = tone[lab].astype(float)
    big = fbm(w, h, [32, 16], seed + 3)
    base += np.where(big > 0.74, 1, 0) - np.where(big < 0.3, 1, 0)
    # small clustered chips (2px) inside stones
    chips = (value_noise(w, h, 3, seed + 9) > 0.86) & ~joint
    base -= chips * 1
    base = np.where(lit, base + 1, base)
    base = np.where(shade, base - 1, base)
    img = paint(base, ramp)
    img[joint] = ramp[0]
    if moss_bottom:
        yy = np.mgrid[0:h, 0:w][0]
        mfield = fbm(w, h, [16, 8, 4], seed + 21)
        ground = (yy / h)  # 0 top, 1 bottom (v=0 is ground after flip)
        mossm = (mfield * 0.9 + ground * 0.75) > 1.08
        jm = joint & ((mfield * 0.7 + ground * 0.6) > 0.72)
        mi = np.clip((mfield * 4).astype(int), 1, 4)
        mi = np.where(np.roll(mossm, 1, 0), mi, np.minimum(mi + 1, 4))  # lit top of moss clumps
        img[mossm] = MOSS[mi[mossm]]
        img[jm] = MOSS[1]
    return img


# ---------------------------------------------------------------- cut stone
def stone_cut(w=64, h=64, ramp=STONE, seed=2):
    r = np.random.default_rng(seed)
    lab = np.zeros((h, w), int); n = 0
    course = 8
    for row in range(h // course):
        off = r.integers(0, 16)
        x = -off
        while x < w:
            L = int(r.choice([12, 14, 16, 18, 20]))
            for xx in range(x, x + L):
                lab[row * course:(row + 1) * course, xx % w] = n
            n += 1; x += L
    joint, lit, shade, _ = edges(lab)
    tone = r.integers(2, 5, n)
    base = tone[lab].astype(float)
    big = fbm(w, h, [16, 8], seed + 5)
    base += (big > 0.72).astype(int) - (big < 0.28).astype(int)
    base = np.where(lit, base + 1, base); base = np.where(shade, base - 1, base)
    img = paint(base, ramp); img[joint] = ramp[1]
    chips = value_noise(w, h, 2, seed + 7) > 0.9
    img[chips & ~joint] = ramp[1]
    return img


# ---------------------------------------------------------------- plaster
def plaster(w=64, h=64, seed=3):
    big = fbm(w, h, [32, 16, 8], seed)
    d = dither_tile(w, h)
    blot = value_noise(w, h, 4, seed + 31)
    idx = np.floor(big * 5.0).astype(int) + (blot > 0.8) * 1 - (blot < 0.15) * 1
    idx = np.clip(idx, 0, 4)
    img = paint(idx, PLASTER)
    # weathered patches: exposed brick cluster
    r = np.random.default_rng(seed)
    for _ in range(2):
        cx, cy = r.integers(0, w), r.integers(0, h)
        for by in range(3):
            for bx in range(2 + (by % 2)):
                x0 = cx + bx * 5 - (by % 2) * 2; y0 = cy + by * 3
                for yy in range(y0, y0 + 2):
                    for xx in range(x0, x0 + 4):
                        img[yy % h, xx % w] = BRICK[(bx + by) % 3]
        # chipped rim around patch
        for _ in range(10):
            img[(cy + r.integers(-1, 9)) % h, (cx + r.integers(-3, 12)) % w] = PLASTER[0]
    # hairline cracks
    for _ in range(3):
        x, y = r.integers(0, w), r.integers(0, h)
        for _ in range(r.integers(4, 9)):
            img[y % h, x % w] = PLASTER[0]
            y += 1; x += r.choice([-1, 0, 0, 1])
    # grime band towards bottom of each 2 m tile is avoided (keeps tiling neutral)
    return img


# ---------------------------------------------------------------- beam wood (grain along V)
def beam_wood(w=16, h=64, ramp=TIMBER, seed=4):
    r = np.random.default_rng(seed)
    col = r.integers(2, 4, w).astype(float)
    # long grain streaks
    streak = value_noise(w, h, 6, seed)  # varies along length
    idx = col[None, :] + (streak > 0.7) - (streak < 0.25)
    for _ in range(5):
        x = r.integers(0, w); y0 = r.integers(0, h); L = r.integers(6, 20)
        for y in range(y0, y0 + L):
            idx[y % h, x] -= 1
    # knots
    for _ in range(2):
        x, y = r.integers(1, w - 1), r.integers(0, h)
        idx[y % h, x] = 0; idx[(y + 1) % h, x] = 1; idx[(y - 1) % h, x] = 4
    img = paint(idx, ramp)
    return img


# ---------------------------------------------------------------- door (1 m x 2 m, not tiling)
def door(w=32, h=64, seed=5):
    r = np.random.default_rng(seed)
    idx = np.zeros((h, w))
    pw = 6
    for p in range(0, w, pw + 0):
        t = r.integers(2, 4)
        idx[:, p:p + pw] = t
        idx[:, p] = 1  # gap shadow
        idx[:, min(p + 1, w - 1)] = t + 1  # lit left edge of plank
        g = value_noise(pw, h, 5, seed + p)
        seg = idx[:, p + 2:p + pw]
        gg = g[:, 2:2 + seg.shape[1]]
        idx[:, p + 2:p + pw] = seg + (gg > 0.72) * 1 - (gg < 0.2) * 1
    img = paint(idx, DOORWOOD)
    # iron straps at 1/5 and 4/5 height with nails
    for sy in (10, 50):
        img[sy:sy + 3, 2:w - 2] = IRON[2]
        img[sy, 2:w - 2] = IRON[3]
        img[sy + 3, 2:w - 2] = DOORWOOD[0]
        for nx in range(4, w - 3, 6):
            img[sy + 1, nx] = IRON[4]
    # ring handle
    hx, hy = w - 8, 33
    for (dx, dy) in [(0, 0), (1, 0), (-1, 1), (2, 1), (-1, 2), (2, 2), (0, 3), (1, 3)]:
        img[hy + dy, hx + dx] = IRON[3] if dy < 2 else IRON[1]
    img[hy - 1, hx:hx + 2] = IRON[4]
    # dark frame edge + threshold wear
    img[:, 0] = DOORWOOD[0]; img[:, -1] = DOORWOOD[0]; img[0, :] = DOORWOOD[0]
    img[h - 2:, :] = DOORWOOD[1]
    return img


# ---------------------------------------------------------------- roof slate / shingle (rows along U, downhill = +V row)
def roof(w=64, h=64, ramp=SLATE, row=6, seed=6, minw=4, maxw=8, moss=True):
    """Slate/shingle rows along U; image-down = downhill. Each slate: lit top-left, dark gap right,
    deep overlap shadow under every row, per-slate tone. Moss only as tiny clusters in the gaps."""
    r = np.random.default_rng(seed)
    idx = np.zeros((h, w)); gap = np.zeros((h, w), bool)
    for ry in range(h // row):
        x = int(r.integers(0, maxw))
        y0 = ry * row
        while x < w + maxw:
            L = int(r.integers(minw, maxw))
            t = r.choice([2, 2, 3, 3, 3, 4])
            drop = 1 if r.random() < 0.3 else 0   # some slates hang 1px lower: ragged row edge
            for k, xx in enumerate(range(x, x + L)):
                X = xx % w
                idx[y0:y0 + row, X] = t
                if k == 0:
                    gap[y0:y0 + row - 1, X] = True
                if k == 1:
                    idx[y0 + 1:y0 + row - 2, X] = t + 1     # lit left edge
                if drop and k < L - 1:
                    idx[(y0 + row) % h, X] = t - 1          # lip over the next row's top
            idx[y0, x % w:(x + L) % w if (x + L) % w > x % w else w] -= 0
            x += L
    yy = np.mgrid[0:h, 0:w][0] % row
    idx = np.where(yy == row - 1, 0, idx)          # overlap shadow
    idx = np.where(yy == 0, idx - 1, idx)          # top of slate shaded by the row above
    idx = np.where(gap, 0, idx)
    big = fbm(w, h, [32, 16], seed + 2)
    idx -= (big < 0.3) * 1          # broad darker patches only (weathering), no extra highlights
    img = paint(idx, ramp)
    if moss:
        m = value_noise(w, h, 3, seed + 11)
        mm = (m > 0.9) & ((yy == row - 1) | gap)
        img[mm] = MOSS[1]
        img[np.roll(mm, -1, 0) & ~mm & (m > 0.93)] = MOSS[2]
    return img


def iron(w=16, h=16, seed=8):
    n = value_noise(w, h, 4, seed)
    idx = 2 + (n > 0.65) * 1 - (n < 0.3) * 1
    img = paint(idx, IRON)
    img[0, :] = IRON[3]; img[:, 0] = IRON[3]
    return img


def cloth(w=32, h=32, seed=9):
    """Awning canvas: cream with muted red stripes 0.25 m wide along V (the slope), soft fold shading, a fine weave."""
    RED_CLOTH = hexramp('#4a1c18', '#6e2a22', '#8c3a2c', '#a64e38')
    yy, xx = np.mgrid[0:h, 0:w]
    stripe = (xx // 8) % 2 == 1
    fold = ((xx % 8) == 0) | ((xx % 8) == 7)
    n = fbm(w, h, [16, 8], seed)
    idx = 3 + (n > 0.62) * 1 - (n < 0.32) * 1 - fold * 1
    img = np.where(stripe[..., None], RED_CLOTH[np.clip(idx - 1, 0, 3)], CLOTH[np.clip(idx, 0, 4)])
    weave = ((xx + yy) % 4 == 0) & (n < 0.45)
    img[weave & ~stripe] = CLOTH[2]
    img[weave & stripe] = RED_CLOTH[1]
    return img


def window(w=32, h=32):
    """1 m x 1 m leaded window: albedo + emissive mask. Frame is geometry; this is the glazing."""
    yy, xx = np.mgrid[0:h, 0:w]
    # inner glow: brighter towards centre-bottom (lit room), stepped
    cx, cy = w / 2, h * 0.62
    d = np.sqrt(((xx - cx) / w) ** 2 + ((yy - cy) / h) ** 2)
    step = np.clip((0.62 - d) * 6.5, 0, 4).astype(int)
    alb = GLOW[np.clip(step, 0, 4)].copy()
    lead = (xx % 8 == 0) | (yy % 10 == 0) | (xx == w - 1) | (yy == h - 1)
    # diamond-ish leading would be noisy at this size; keep a calm 4x3 grid
    alb[lead] = IRON[1]
    # mullion cross (thicker)
    alb[:, w // 2 - 1:w // 2 + 1] = TIMBER[1]
    alb[h // 2 - 1:h // 2 + 1, :] = TIMBER[1]
    emi = alb.copy()
    emi[lead] = 0
    emi[:, w // 2 - 1:w // 2 + 1] = 0; emi[h // 2 - 1:h // 2 + 1, :] = 0
    return alb, emi


def sign(w=32, h=24):
    """Hanging sign board 1 m x 0.75 m: planks + painted tankard emblem (inn/tavern)."""
    img = door(w, h * 3)[24:24 + h].copy()
    img[0:2, :] = TIMBER[1]; img[-2:, :] = TIMBER[1]; img[:, 0:2] = TIMBER[1]; img[:, -2:] = TIMBER[1]
    img[1, 1:-1] = TIMBER[4]
    # tankard emblem (cream paint)
    paint_c = PLASTER[5]; sh = PLASTER[1]
    for y in range(7, 18):
        img[y, 11:19] = paint_c
        img[y, 18] = sh
    img[6, 10:20] = PLASTER[4]   # foam rim
    img[5, 11:15] = PLASTER[5]; img[5, 16:18] = PLASTER[5]
    for y in (9, 15):
        img[y, 11:19] = sh  # bands
    for y in range(9, 16):
        img[y, 20] = paint_c
    img[9, 19] = paint_c; img[15, 19] = paint_c
    return img



# ---------------------------------------------------------------- planks (vertical boards, tiling 2 m)
PLANK = hexramp('#21170f', '#2e2016', '#3d2b1d', '#4d3725', '#5d442d', '#6e5236')


def planks(w=64, h=64, seed=13):
    r = np.random.default_rng(seed)
    idx = np.zeros((h, w))
    x = 0
    while x < w:
        pw = int(r.choice([7, 8, 8, 9]))
        pw = min(pw, w - x) if w - x >= 5 else w - x
        t = r.choice([2, 2, 3, 3, 4])
        g = value_noise(pw, h, 6, seed + x)
        idx[:, x:x + pw] = t + (g > 0.72) * 1 - (g < 0.22) * 1
        idx[:, x] = 0                                   # gap
        if pw > 2:
            idx[:, x + 1] = np.minimum(idx[:, x + 1] + 1, 5)  # lit left edge
        # board ends: some boards are two pieces
        if r.random() < 0.5:
            y = int(r.integers(4, h - 4))
            idx[y, x:x + pw] = 0
            idx[(y + 1) % h, x:x + pw] = np.minimum(idx[(y + 1) % h, x:x + pw] + 1, 5)
        for ny in (int(r.integers(0, 6)), int(r.integers(32, 38))):   # nails
            if pw > 4:
                idx[ny, x + 2] = 0; idx[ny, x + pw - 2] = 0
        x += pw
    return paint(idx, PLANK)


def tilled(w=64, h=64, seed=14):
    """Tilled field soil: furrows along U every 16 px (0.5 m), lit ridge tops, dark troughs."""
    DIRT = hexramp('#2a1d13', '#3a281a', '#4c3522', '#5f432b', '#735335', '#87633e')
    yy, xx = np.mgrid[0:h, 0:w]
    phase = yy % 16
    base = np.select([phase < 3, phase < 6, phase < 11, phase < 14], [1, 3, 4, 3], 2).astype(float)
    n = fbm(w, h, [16, 8], seed)
    base += (n > 0.7) * 1 - (n < 0.28) * 1
    clods = value_noise(w, h, 2, seed + 3) > 0.88
    base = np.where(clods, base + 1, base)
    img = paint(base, DIRT)
    # a few weed tufts
    m = value_noise(w, h, 3, seed + 5) > 0.93
    img[m & (phase > 4) & (phase < 10)] = MOSS[2]
    return img


def water(w=64, h=64, seed=15):
    """Stream water (style guide teal ramp): calm bands with sparse light ripples."""
    WATER = hexramp('#0d2b2e', '#123A3C', '#185052', '#1E6565', '#3f8f86', '#7BC9B8')
    n = fbm(w, h, [32, 16], seed)
    idx = 1 + (n > 0.45) * 1 + (n > 0.62) * 1
    yy, xx = np.mgrid[0:h, 0:w]
    rip = (value_noise(w, h, 4, seed + 2) > 0.78) & ((yy % 6) == 0)
    idx = np.where(rip, 4, idx)
    sp = (value_noise(w, h, 2, seed + 4) > 0.93) & ((yy % 6) == 1)
    idx = np.where(sp, 5, idx)
    return paint(idx, WATER)


def save(img, path, scale=1):
    im = Image.fromarray(img.astype(np.uint8), 'RGB')
    im.save(path)
    if scale > 1:
        im.resize((im.width * scale, im.height * scale), Image.NEAREST).save(path.replace('.png', '_x%d.png' % scale))


def main(out):
    os.makedirs(out, exist_ok=True)
    tex = {
        'MV_Stone_Field.png': stone_field(),
        'MV_Stone_Cut.png': stone_cut(),
        'MV_Plaster_Warm.png': plaster(),
        'MV_Wood_DarkBeam.png': beam_wood(),
        'MV_Wood_Door.png': door(),
        'MV_Roof_SlateDark.png': roof(64, 48),
        'MV_Roof_ShingleDark.png': roof(ramp=SHINGLE, row=8, minw=3, maxw=6, seed=12, moss=False),
        'MV_Metal_IronDark.png': iron(),
        'MV_Cloth_WarmNeutral.png': cloth(),
        'MV_Sign_Tavern.png': sign(),
        'MV_Wood_Planks.png': planks(),
        'MV_Soil_Tilled.png': tilled(),
        'MV_Water_Stream.png': water(),
        'MV_Stone_Chimney.png': stone_field(64, 64, moss_bottom=False, ramp=STONE_COOL, seed=33),
    }
    a, e = window()
    tex['MV_Window_Glass.png'] = a
    tex['MV_Window_Glass_Emissive.png'] = e
    for k, v in tex.items():
        save(v, os.path.join(out, k))
    # contact sheet for review
    sheet_items = list(tex.items())
    S = 4
    W = sum(v.shape[1] * S + 8 for _, v in sheet_items[:6])
    rows = [sheet_items[:6], sheet_items[6:]]
    H = sum(max(v.shape[0] for _, v in rw) * S + 8 for rw in rows)
    sheet = Image.new('RGB', (max(W, 900), H), (40, 36, 48))
    y = 0
    for rw in rows:
        x = 0
        for _, v in rw:
            im = Image.fromarray(v.astype(np.uint8)).resize((v.shape[1] * S, v.shape[0] * S), Image.NEAREST)
            sheet.paste(im, (x, y)); x += im.width + 8
        y += max(v.shape[0] for _, v in rw) * S + 8
    sheet.save(os.path.join(out, '_contact.png'))


if __name__ == '__main__':
    main(sys.argv[1])
