
import bpy, sys, os
import numpy as np

out = sys.argv[sys.argv.index("--") + 1]
sheet_path = sys.argv[sys.argv.index("--") + 2]


def ramp(*hs):
    return np.array([[int(h[i:i + 2], 16) for i in (1, 3, 5)] for h in hs], dtype=np.uint8)


def value_noise(w, h, cell, seed):
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


def paint(idx, rp):
    return rp[np.clip(idx, 0, len(rp) - 1).astype(int)]


STRAW = ramp('#251c10', '#352917', '#4a3a1f', '#614d29', '#7a6335', '#93793f')
DIRT = ramp('#2d2016', '#3d2c1d', '#4f3a26', '#634830', '#7a5a3b', '#8f6c46')
MOSSC = np.array([63, 85, 39], dtype=np.uint8)


def thatch(w=64, h=64, seed=21):
    """Thatch: 0.5 m bundle courses (16 px), dark gap under each course edge, vertical straw streaks."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]
    band = 16
    b = yy // band; ph = yy % band
    base = np.select([ph < 2, ph < 5, ph < 11], [0, 2, 3], 4).astype(float)
    edge = np.repeat(r.integers(0, 3, (h // band, w // 2)), 2, axis=1)[b, xx]
    base = np.where(ph < edge, 0, base)
    base = np.where((ph >= edge) & (ph < edge + 1), 1, base)
    streak = np.repeat(r.integers(-1, 1, (h // band, w // 2)), 2, axis=1)[b, xx]
    base = base + np.where(base > 1, streak, 0)
    base += (r.random((h, w)) > 0.95) * 1 - (r.random((h, w)) > 0.95) * 1
    n = fbm(w, h, [16, 8], seed)
    base += (n > 0.7) * 1 - (n < 0.28) * 1
    img = paint(base, STRAW)
    m = (value_noise(w, h, 4, seed + 7) > 0.9) & (base >= 3)
    img[m] = MOSSC
    return img


def dirt_road(w=64, h=64, seed=31):
    """Packed dirt road: quiet base, a few lit pebbles with a shadow pixel, rare grass tufts."""
    yy, xx = np.mgrid[0:h, 0:w]
    n = fbm(w, h, [16, 8], seed)
    base = np.full((h, w), 2.0) + (n > 0.66) * 1 - (n < 0.34) * 1
    clods = value_noise(w, h, 3, seed + 3) > 0.9
    base = np.where(clods, base + 1, base)
    p = value_noise(w, h, 4, seed + 2) > 0.9
    sh = np.roll(p, 1, 0) & ~p
    base = np.where(p, base + 2, base)
    base = np.where(sh, base - 1, base)
    ruts = (np.abs(yy - 16) < 3) | (np.abs(yy - 48) < 3)
    base = np.where(ruts & (value_noise(w, h, 6, seed + 11) > 0.3), base - 1, base)
    img = paint(base, DIRT)
    t = value_noise(w, h, 4, seed + 5) > 0.94
    img[t] = MOSSC
    return img


def save(arr, path):
    h, w, _ = arr.shape
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=True)
    img.colorspace_settings.name = 'Non-Color'
    buf = np.ones((h, w, 4), np.float32)
    buf[..., :3] = arr / 255.0
    img.pixels.foreach_set(buf[::-1].ravel())
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()


def load(path):
    img = bpy.data.images.load(path)
    img.colorspace_settings.name = 'Non-Color'
    w, h = img.size
    a = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(a)
    a = a.reshape(h, w, 4)[::-1, :, :3]
    return np.round(a * 255).astype(np.uint8)


tex = {"MV_Roof_Thatch.png": thatch(), "MV_Dirt_Road.png": dirt_road()}
for k, v in tex.items():
    save(v, os.path.join(out, k))
# round-trip check + contact sheet (new + the soil / water already in the kit)
back = {k: load(os.path.join(out, k)) for k in tex}
ok = all(np.abs(back[k].astype(int) - tex[k].astype(int)).max() <= 1 for k in tex)
items = [back["MV_Roof_Thatch.png"], back["MV_Dirt_Road.png"],
         load(os.path.join(out, "MV_Soil_Tilled.png")), load(os.path.join(out, "MV_Water_Stream.png"))]
S = 4
sheet = np.full((64 * S + 16, (64 * S + 8) * len(items) + 8, 3), 40, np.uint8)
for i, im in enumerate(items):
    big = np.repeat(np.repeat(im, S, 0), S, 1)
    x = 8 + i * (64 * S + 8)
    sheet[8:8 + 64 * S, x:x + 64 * S] = big
save(sheet, sheet_path)
print("RESULT::roundtrip_ok=%s" % ok)
