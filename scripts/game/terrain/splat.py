"""Pixel splat for the terrain tops (Terrain plan, "Surface"): the reference for terrain.frag.

Per art pixel (1/32 m): sample the RGBA weight map (bilinear at the pixel's centre), add clustered value
noise per layer scaled by the layer's EdgeRoughness (weight units), and the highest score wins; that layer's
albedo is sampled at the pixel. Where two weights differ by more than the roughness the result can't flip,
so painted areas stay clean and only borders go ragged.

Blending (Oct 31; terrain.frag follows bake_blended): each layer's score also gets its texture's brightness
(HeightBlend: bright blades and stones reach into the neighbour, dark gaps let it through), and inside the blend
band (Blend, weight units) the winner's colour is mixed with the runner-up's in steps of 25 % (50 % at a tie).
The CPU winner (winners(), TerrainMesher::GetTopLayerAt: fringe and decor) has no textures and ignores the height.
"""

from dataclasses import dataclass

import numpy as np
from PIL import Image

from terrain_mesher import ART_PPM, TAG_SPLAT_CLUSTER, TAG_SPLAT_PIXEL, TAG_FRINGE, hash01, hash_u32

M32 = np.uint64(0xFFFFFFFF)


def hash_u32_np(*values):
    """numpy version of terrain_mesher.hash_u32, element-wise (identical results)."""
    shape = np.broadcast(*[np.asarray(v) for v in values]).shape
    h = np.full(shape, 0x811C9DC5, dtype=np.uint64)
    for v in values:
        v = np.asarray(v).astype(np.int64).astype(np.uint64) & M32
        h ^= v
        h = (h * np.uint64(0x01000193)) & M32
        h ^= h >> np.uint64(15)
        h = (h * np.uint64(0x2C1B3C6D)) & M32
        h ^= h >> np.uint64(12)
        h = (h * np.uint64(0x297A2D39)) & M32
        h ^= h >> np.uint64(15)
    return h


def hash01_np(*values):
    return hash_u32_np(*values).astype(np.float64) / 4294967296.0


@dataclass
class Layer:
    name: str
    texture: str          # path to the albedo PNG
    tiling: tuple         # repeats per metre (the .hymat's Tiling)
    tint: tuple = (1.0, 1.0, 1.0)
    roughness: float = 0.3
    blend: float = 0.0         # width of the mixed band, weight units
    height_blend: float = 0.0  # how much the texture's brightness raises the score


@dataclass
class SplatSettings:
    resolution: int = 8         # weight map texels per metre
    noise_cell_px: float = 3.0  # size of the noise clusters, art pixels
    pixel_noise: float = 0.2    # share of per-pixel noise in the edge noise (0 = only clusters)


def sample_weights(weights, res, x, y):
    """Bilinear sample of the weight map (h, w, 4), texel centres at (k + 0.5) / res metres."""
    h, w, _ = weights.shape
    fx = np.clip(x * res - 0.5, 0.0, w - 1.0)
    fy = np.clip(y * res - 0.5, 0.0, h - 1.0)
    x0 = np.floor(fx).astype(int)
    y0 = np.floor(fy).astype(int)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + 1, h - 1)
    ax = (fx - x0)[..., None]
    ay = (fy - y0)[..., None]
    top = weights[y0, x0] * (1 - ax) + weights[y0, x1] * ax
    bot = weights[y1, x0] * (1 - ax) + weights[y1, x1] * ax
    return top * (1 - ay) + bot * ay


def edge_noise(seed, ix, iy, layer, settings):
    """Clustered value noise (smooth between lattice points every noise_cell_px art pixels) plus a little
    per-pixel noise, 0..1, hashed from global art-pixel positions."""
    c = settings.noise_cell_px
    gx = ix / c
    gy = iy / c
    x0 = np.floor(gx).astype(np.int64)
    y0 = np.floor(gy).astype(np.int64)
    fx = gx - x0
    fy = gy - y0
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    v00 = hash01_np(seed, x0, y0, layer, TAG_SPLAT_CLUSTER)
    v10 = hash01_np(seed, x0 + 1, y0, layer, TAG_SPLAT_CLUSTER)
    v01 = hash01_np(seed, x0, y0 + 1, layer, TAG_SPLAT_CLUSTER)
    v11 = hash01_np(seed, x0 + 1, y0 + 1, layer, TAG_SPLAT_CLUSTER)
    cluster = (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy
    pixel = hash01_np(seed, ix, iy, layer, TAG_SPLAT_PIXEL)
    return (1 - settings.pixel_noise) * cluster + settings.pixel_noise * pixel


def winners(weights, layers, seed, settings, ix, iy):
    """Winning layer index per art pixel (ix, iy arrays of global art-pixel indices)."""
    x = (ix + 0.5) / ART_PPM
    y = (iy + 0.5) / ART_PPM
    w = sample_weights(weights, settings.resolution, x, y)
    scores = []
    for k, layer in enumerate(layers):
        n = edge_noise(seed, ix, iy, k, settings)
        scores.append(w[..., k] + (n - 0.5) * layer.roughness)
    return np.argmax(np.stack(scores, axis=-1), axis=-1)


def load_texture(path):
    im = Image.open(path).convert("RGB")
    return np.asarray(im).astype(np.float64) / 255.0


def bake(weights, layers, seed, settings, width_m, height_m):
    """The tops' colour at one texel per art pixel over [0, width] x [0, height] metres, as an image with
    row 0 at the north edge (y = height). Returns (PIL image, winners array [iy, ix] south-up)."""
    nx = int(round(width_m * ART_PPM))
    ny = int(round(height_m * ART_PPM))
    ix, iy = np.meshgrid(np.arange(nx), np.arange(ny))
    win = winners(weights, layers, seed, settings, ix, iy)
    out = np.zeros((ny, nx, 3))
    for k, layer in enumerate(layers):
        tex = load_texture(layer.texture)
        th, tw, _ = tex.shape
        x = (ix + 0.5) / ART_PPM
        y = (iy + 0.5) / ART_PPM
        tx = np.floor(x * layer.tiling[0] * tw).astype(int) % tw
        ty = np.floor(y * layer.tiling[1] * th).astype(int) % th
        colour = tex[th - 1 - ty, tx] * np.asarray(layer.tint)
        mask = win == k
        out[mask] = colour[mask]
    img = Image.fromarray((np.clip(out[::-1], 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
    return img, win


def bake_blended(weights, layers, seed, settings, width_m, height_m):
    """bake() with Blend and HeightBlend, as terrain.frag draws it. Returns the PIL image (row 0 north)."""
    nx = int(round(width_m * ART_PPM))
    ny = int(round(height_m * ART_PPM))
    ix, iy = np.meshgrid(np.arange(nx), np.arange(ny))
    x = (ix + 0.5) / ART_PPM
    y = (iy + 0.5) / ART_PPM
    w = sample_weights(weights, settings.resolution, x, y)
    colours = []
    scores = []
    for k, layer in enumerate(layers):
        tex = load_texture(layer.texture)
        th, tw, _ = tex.shape
        tx = np.floor(x * layer.tiling[0] * tw).astype(int) % tw
        ty = np.floor(y * layer.tiling[1] * th).astype(int) % th
        raw = tex[th - 1 - ty, tx]
        height = raw @ np.array([0.299, 0.587, 0.114])
        colours.append(raw * np.asarray(layer.tint))
        n = edge_noise(seed, ix, iy, k, settings)
        scores.append(w[..., k] + (n - 0.5) * layer.roughness + (height - 0.5) * layer.height_blend)
    s = np.stack(scores, axis=-1)
    c = np.stack(colours, axis=-2)
    order = np.argsort(-s, axis=-1, kind="stable")
    best = order[..., 0]
    second = order[..., 1] if len(layers) > 1 else best
    sb = np.take_along_axis(s, best[..., None], -1)[..., 0]
    ss = np.take_along_axis(s, second[..., None], -1)[..., 0]
    cb = np.take_along_axis(c, best[..., None, None], -2)[..., 0, :]
    cs = np.take_along_axis(c, second[..., None, None], -2)[..., 0, :]
    blend = np.asarray([layer.blend for layer in layers])
    band = 0.5 * (blend[best] + blend[second])
    share = np.where(band > 0, np.clip(0.5 + 0.5 * (sb - ss) / np.maximum(band, 1e-9), 0.5, 1.0), 1.0)
    share = np.floor(share * 4.0 + 0.5) / 4.0
    out = cs * (1 - share[..., None]) + cb * share[..., None]
    return Image.fromarray((np.clip(out[::-1], 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")


def winner_lookup(win):
    """A function (x, y) -> winning layer index at that world point, from bake()'s winners."""
    ny, nx = win.shape

    def lookup(x, y):
        ix = min(max(int(x * ART_PPM), 0), nx - 1)
        iy = min(max(int(y * ART_PPM), 0), ny - 1)
        return int(win[iy, ix])
    return lookup


def make_fringe(grass_texture, length_px, seed, width_px=64):
    """The hanging grass strip: width_px art pixels wide (2 m), length_px tall, top row attached to the lip.
    Each column hangs a clustered random length; colours come from the grass texture, darker lower down.
    Returns an RGBA image (row 0 = top)."""
    tex = load_texture(grass_texture)
    th, tw, _ = tex.shape
    length_px = max(1, int(round(length_px)))
    img = np.zeros((length_px, width_px, 4))
    for c in range(width_px):
        # Clusters of 4 columns, wrapping at width_px so the strip tiles without a seam.
        cells = max(1, width_px // 4)
        g = c / 4.0
        g0 = int(np.floor(g))
        f = g - g0
        a = hash01(seed, g0 % cells, 0, 0, TAG_FRINGE)
        b = hash01(seed, (g0 + 1) % cells, 0, 0, TAG_FRINGE)
        v = a * (1 - f) + b * f
        v = 0.75 * v + 0.25 * hash01(seed, c, 1, 0, TAG_FRINGE)
        n = int(round(v * length_px))
        for r in range(n):
            col = tex[(r * 7 + c * 3) % th, (c * 5) % tw]
            shade = 0.82 - 0.22 * (r / max(1, length_px - 1))
            img[r, c, :3] = col * shade
            img[r, c, 3] = 1.0
    return Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")
