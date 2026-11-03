"""Reads and writes .hyterrain files (Terrain plan, "Data"), the same format as Hydro's TerrainSerializer:

    Handle: <u64>
    Terrain:   readable settings
    Data:      Levels, Ramps, WallMaterial (run-length pairs, base64 wrapped at 76) and Weights (PNG, north row first)

Arrays are numpy, indexed [y, x] from the south-west corner: levels int8 [corners_y, corners_x], ramps and walls
uint8 [cells_y, cells_x], weights uint8 [texels_y, texels_x, 4]. The editor rewrites the file in its own number
formatting the first time it saves the terrain; the data stays the same.
"""

import base64
import io
import random

import numpy as np
from PIL import Image

DECOR_PLACES = ("Layer", "LayerBorder", "WallTop", "WallFoot")
DECOR_SHAPES = ("Upright", "Flat", "Stone")
ANY_WALL_LAYER = 255


def new_handle(rng=None):
    rng = rng or random.SystemRandom()
    return rng.randrange(1 << 40, (1 << 63) - 1)


def run_length(values):
    """Pairs of (count 1..255, value), like TerrainSerializer::EncodeRunLength."""
    flat = np.asarray(values, dtype=np.uint8).ravel()
    out = bytearray()
    i = 0
    n = len(flat)
    while i < n:
        v = flat[i]
        count = 1
        while i + count < n and flat[i + count] == v and count < 255:
            count += 1
        out.append(count)
        out.append(int(v))
        i += count
    return bytes(out)


def un_run_length(data, count):
    out = bytearray()
    for k in range(0, len(data), 2):
        out.extend([data[k + 1]] * data[k])
    if len(out) != count:
        raise ValueError("run-length block has %d values, expected %d" % (len(out), count))
    return np.frombuffer(bytes(out), dtype=np.uint8)


def wrap76(text):
    return "\n".join(text[k:k + 76] for k in range(0, len(text), 76)) or ""


def b64_block(data):
    return wrap76(base64.b64encode(data).decode("ascii"))


def png_bytes(weights):
    """The weight map as PNG bytes, north row first."""
    north = np.ascontiguousarray(weights[::-1])
    buffer = io.BytesIO()
    Image.fromarray(north, "RGBA").save(buffer, format="PNG", optimize=False, compress_level=9)
    return buffer.getvalue()


def fmt(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        s = ("%.6f" % v).rstrip("0").rstrip(".")
        return s if s not in ("", "-0") else "0"
    return str(v)


def vec(values):
    return "[" + ", ".join(fmt(float(v)) for v in values) + "]"


class TerrainSettings:
    """TerrainSettings with the approved TR1 defaults."""

    def __init__(self):
        self.cell_size = 0.5
        self.step_height = 0.25
        self.base_level = 0
        self.seed = 1234
        self.splat_resolution = 8
        self.jitter = (0.38, 0.75, 2.0)        # amount, smooth share, scale (m)
        self.edge_noise = (2.5, 0.35)          # cluster size (px), pixel noise
        self.layers = []                       # dicts: material, roughness, blend, height_blend, fringe, fringe_length
        self.wall_layers = []                  # dicts: material, variation, caps, slope
        self.lip = (3.0, 2.0)                  # width, face height (px)
        self.wall_profile = (1.0, 1.5, 0.35)   # lean (px/m), noise (px), light (0..1: walls lit as if facing partly up)
        self.cap_material = 0
        self.caps = dict(pitch=0.28, joint=0.015, depth=0.2, height=(0.06, 0.08), size_jitter=0.04,
                         position_jitter=0.015, yaw_jitter=3.0, missing=0.08)
        self.decor = []                        # dicts: place, layer, shape, density, scale, sprites

    def add_layer(self, material, roughness=0.9, fringe=0, fringe_length=7.0, blend=0.5, height_blend=1.0):
        self.layers.append(dict(material=material, roughness=roughness, blend=blend, height_blend=height_blend, fringe=fringe,
                                fringe_length=fringe_length))

    def add_wall_layer(self, material, variation=1.0, caps=False, slope=0.0):
        self.wall_layers.append(dict(material=material, variation=variation, caps=caps, slope=slope))

    def add_decor(self, place, layer, shape, density, scale, sprites):
        self.decor.append(dict(place=place, layer=layer, shape=shape, density=density, scale=scale, sprites=list(sprites)))


def write(path, handle, settings, levels, ramps, walls, weights):
    levels = np.asarray(levels)
    ny, nx = levels.shape
    assert ramps.shape == (ny - 1, nx - 1) and walls.shape == (ny - 1, nx - 1)
    assert weights.dtype == np.uint8 and weights.shape[2] == 4
    s = settings
    lines = ["Handle: %d" % handle, "Terrain:"]
    add = lines.append
    add("  Version: 1")
    add("  CellSize: %s" % fmt(s.cell_size))
    add("  StepHeight: %s" % fmt(s.step_height))
    add("  Size: [%d, %d]" % (nx, ny))
    add("  BaseLevel: %d" % s.base_level)
    add("  Seed: %d" % s.seed)
    add("  SplatResolution: %d" % s.splat_resolution)
    add("  Jitter:")
    add("    Amount: %s" % fmt(s.jitter[0]))
    add("    Smooth: %s" % fmt(s.jitter[1]))
    add("    Scale: %s" % fmt(s.jitter[2]))
    add("  EdgeNoise:")
    add("    ClusterSize: %s" % fmt(s.edge_noise[0]))
    add("    PixelNoise: %s" % fmt(s.edge_noise[1]))
    add("  Layers:")
    for layer in s.layers:
        add("    - Material: %d" % layer["material"])
        add("      EdgeRoughness: %s" % fmt(layer["roughness"]))
        add("      Blend: %s" % fmt(layer.get("blend", 0.0)))
        add("      HeightBlend: %s" % fmt(layer.get("height_blend", 0.0)))
        add("      Fringe: %d" % layer["fringe"])
        add("      FringeLength: %s" % fmt(layer["fringe_length"]))
    if s.wall_layers:
        add("  WallLayers:")
        for wall in s.wall_layers:
            add("    - Material: %d" % wall["material"])
            add("      Variation: %s" % fmt(wall["variation"]))
            add("      CapStones: %s" % fmt(bool(wall["caps"])))
            add("      Slope: %s" % fmt(wall.get("slope", 0.0)))
    else:
        add("  WallLayers:")
        add("    []")
    add("  Lip:")
    add("    Width: %s" % fmt(s.lip[0]))
    add("    FaceHeight: %s" % fmt(s.lip[1]))
    add("  WallProfile:")
    add("    Lean: %s" % fmt(s.wall_profile[0]))
    add("    Noise: %s" % fmt(s.wall_profile[1]))
    add("    Light: %s" % fmt(s.wall_profile[2] if len(s.wall_profile) > 2 else 0.35))
    c = s.caps
    add("  CapStones:")
    add("    Material: %d" % s.cap_material)
    add("    Pitch: %s" % fmt(c["pitch"]))
    add("    Joint: %s" % fmt(c["joint"]))
    add("    Depth: %s" % fmt(c["depth"]))
    add("    Height: %s" % vec(c["height"]))
    add("    SizeJitter: %s" % fmt(c["size_jitter"]))
    add("    PositionJitter: %s" % fmt(c["position_jitter"]))
    add("    YawJitter: %s" % fmt(c["yaw_jitter"]))
    add("    Missing: %s" % fmt(c["missing"]))
    if s.decor:
        add("  Decor:")
        for rule in s.decor:
            add("    - Place: %s" % rule["place"])
            add("      Layer: %d" % rule["layer"])
            add("      Shape: %s" % rule["shape"])
            add("      Density: %s" % fmt(rule["density"]))
            add("      Scale: %s" % vec(rule["scale"]))
            add("      Sprites: [%s]" % ", ".join(str(h) for h in rule["sprites"]))
    else:
        add("  Decor:")
        add("    []")
    add("Data:")
    for key, data in (("Levels", run_length(levels.astype(np.int8).view(np.uint8))),
                      ("Ramps", run_length(ramps)),
                      ("WallMaterial", run_length(walls)),
                      ("Weights", png_bytes(weights))):
        add("  %s: |" % key)
        for row in b64_block(data).split("\n"):
            add("    " + row)
    text = "\n".join(lines) + "\n"
    with open(path, "w", newline="\n") as f:
        f.write(text)


def read(path):
    """(handle, raw YAML dict, levels, ramps, walls, weights)."""
    import yaml
    with open(path) as f:
        doc = yaml.safe_load(f)
    nx, ny = doc["Terrain"]["Size"]
    data = doc["Data"]
    levels = un_run_length(base64.b64decode(data["Levels"]), nx * ny).view(np.int8).reshape(ny, nx)
    ramps = un_run_length(base64.b64decode(data["Ramps"]), (nx - 1) * (ny - 1)).reshape(ny - 1, nx - 1)
    walls = un_run_length(base64.b64decode(data["WallMaterial"]), (nx - 1) * (ny - 1)).reshape(ny - 1, nx - 1)
    weights = np.asarray(Image.open(io.BytesIO(base64.b64decode(data["Weights"]))).convert("RGBA"))[::-1].copy()
    return doc["Handle"], doc, levels, ramps, walls, weights
