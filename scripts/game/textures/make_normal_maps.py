"""Makes tangent-space normal maps for the albedo textures of Standard materials and sets them on the materials.

Pixel-art textures have no height data, so the height is read from the albedo itself: brightness, slightly smoothed
(dark mortar and cracks sink, bright stones and planks rise). The slopes are scaled per texture so a low-contrast
texture still gets some relief and a high-contrast one isn't all spikes. One pixel of the map per pixel of the
albedo, so the lighting stays on the art's pixel grid (the material samples it with its own filter, Nearest by default).

Output, for each albedo "<name>.png" a material uses:
    <name>_N.png          the normal map (RGB: tangent space, green up the image as in glTF / OpenGL; A: 255)
    <name>_N.png.hymeta   its asset handle, imported as RGBA (data, not SRGB), the albedo's filter and wrap
and each material (.hymat) using that albedo gets "NormalHandle" and "NormalStrength". Materials and maps that are
already set up are left alone (use --force to make the maps again; a material's existing NormalHandle is kept).

Which materials: those reachable from the given scenes (directly, or through models, prefabs and terrains), or every
material with --all. Textures that are mostly flat colour (palette atlases, single-colour swatches) are skipped: their
"edges" are UV island borders, not relief.

Usage (from the repository root):
    python scripts/game/textures/make_normal_maps.py                         # the test bed scenes
    python scripts/game/textures/make_normal_maps.py --scenes ShrineGlade    # one scene
    python scripts/game/textures/make_normal_maps.py --all                   # every material in the project
    python scripts/game/textures/make_normal_maps.py --dry-run               # list what it would do
    python scripts/game/textures/make_normal_maps.py --preview out.png       # also a lit before/after sheet

Needs Pillow and numpy. Run the editor afterwards (or let it notice the new files); the materials reload on their own.
"""

import argparse
import os
import random
import re
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ASSETS = os.path.join(ROOT, "Oxygen", "project", "Assets")

DEFAULT_SCENES = ["ShrineGlade", "FarmVillage", "FarmVillageInn", "FarmVillageInnUpstairs", "Battle"]
SUFFIX = "_N"

# Files whose handle is on their first line ("Handle: <n>"); everything else (images, models) has a .hymeta sidecar.
OWN_HANDLE_EXTENSIONS = {".hyscene", ".hyprefab", ".hymat", ".hyanim", ".hydef", ".hytable", ".hyterrain", ".hyprofile", ".hyfont"}
# Files whose handles lead to more assets (the closure from a scene walks through them).
REFERENCING_EXTENSIONS = {".hyscene", ".hyprefab", ".hymat", ".hyterrain", ".hymeta"}

HANDLE_LINE = re.compile(r"^Handle:\s*(\d+)\s*$", re.MULTILINE)
NUMBER = re.compile(r"(?<![\d.])(\d{6,20})(?![\d.])")


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        return f.read()


def write_text(path, text, newline):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text.replace("\r\n", "\n").replace("\n", newline))


def newline_of(text):
    return "\r\n" if "\r\n" in text else "\n"


def build_handle_index():
    """handle -> absolute path of the asset, for every asset under Assets/."""
    index = {}
    for folder, _, files in os.walk(ASSETS):
        for name in files:
            path = os.path.join(folder, name)
            ext = os.path.splitext(name)[1].lower()
            if ext == ".hymeta":
                target = path[: -len(".hymeta")]
                match = HANDLE_LINE.search(read_text(path)[:400])
                if match and os.path.exists(target):
                    index[int(match.group(1))] = target
            elif ext in OWN_HANDLE_EXTENSIONS:
                with open(path, "r", encoding="utf-8", errors="replace") as f:
                    first = f.readline()
                match = HANDLE_LINE.match(first.strip())
                if match:
                    index[int(match.group(1))] = path
    return index


def references_of(path, index):
    """The asset paths a file refers to by handle (its own handle excluded); a model's through its .hymeta."""
    texts = []
    ext = os.path.splitext(path)[1].lower()
    if ext in REFERENCING_EXTENSIONS:
        texts.append(read_text(path))
    sidecar = path + ".hymeta"
    if os.path.exists(sidecar):
        texts.append(read_text(sidecar))
    found = set()
    for text in texts:
        own = HANDLE_LINE.search(text[:400])
        own_handle = int(own.group(1)) if own else None
        for match in NUMBER.finditer(text):
            handle = int(match.group(1))
            if handle != own_handle and handle in index:
                found.add(index[handle])
    return found


def materials_from_scenes(scene_names, index):
    scene_folder = os.path.join(ASSETS, "Scenes")
    queue = []
    for name in scene_names:
        path = os.path.join(scene_folder, name if name.endswith(".hyscene") else name + ".hyscene")
        if os.path.exists(path) is False:
            print(f"warning: no scene {path}")
            continue
        queue.append(path)
    seen = set(queue)
    materials = set()
    while queue:
        path = queue.pop()
        if path.lower().endswith(".hymat"):
            materials.add(path)
        for ref in references_of(path, index):
            ext = os.path.splitext(ref)[1].lower()
            # Scenes linked from a scene (doors, transitions) are not followed: only what this scene draws.
            if ref in seen or ext == ".hyscene":
                continue
            if ext in {".hymat", ".hyprefab", ".hyterrain", ".glb", ".gltf"}:
                seen.add(ref)
                queue.append(ref)
    return materials


def all_materials():
    found = set()
    for folder, _, files in os.walk(ASSETS):
        for name in files:
            if name.lower().endswith(".hymat"):
                found.add(os.path.join(folder, name))
    return found


def material_field(text, key):
    match = re.search(rf"^\s*{key}:\s*(\S+)\s*$", text, re.MULTILINE)
    return match.group(1) if match else None


def texture_import(path):
    """(filter, wrap, format, sprite mode) from an image's .hymeta; the importer's defaults when it has none."""
    result = {"Filter": "Nearest", "Wrap": "Repeat", "Format": "SRGB", "SpriteMode": "Single", "PixelsPerUnit": "32"}
    sidecar = path + ".hymeta"
    if os.path.exists(sidecar):
        text = read_text(sidecar)
        for key in result:
            value = material_field(text, key)
            if value:
                result[key] = value
    return result


def srgb_to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def neighbour(a, dy, dx, wrap):
    if wrap:
        return np.roll(np.roll(a, -dy, axis=0), -dx, axis=1)
    padded = np.pad(a, 1, mode="edge")
    h, w = a.shape
    return padded[1 + dy : 1 + dy + h, 1 + dx : 1 + dx + w]


def height_from_albedo(rgba, wrap):
    """0..1 height per pixel: perceived brightness, a little smoothed so single noisy pixels don't become spikes;
    transparent pixels are the lowest."""
    rgb = rgba[..., :3].astype(np.float64) / 255.0
    alpha = rgba[..., 3].astype(np.float64) / 255.0
    luminance = srgb_to_linear(rgb) @ np.array([0.2126, 0.7152, 0.0722])
    height = np.sqrt(np.clip(luminance, 0.0, 1.0))  # back towards perceptual: mortar and cracks read as depth
    height = height * alpha
    # 3 x 3 binomial blur, mixed half and half with the sharp height: the pixel grid stays, the noise softens.
    blurred = np.zeros_like(height)
    for dy, wy in ((-1, 1), (0, 2), (1, 1)):
        for dx, wx in ((-1, 1), (0, 2), (1, 1)):
            blurred += neighbour(height, dy, dx, wrap) * (wy * wx)
    blurred /= 16.0
    return 0.5 * height + 0.5 * blurred


def slopes(height, wrap):
    """Sobel slopes per pixel: dh/dx to the right, dh/dy down the image."""
    n = lambda dy, dx: neighbour(height, dy, dx, wrap)
    gx = (n(-1, 1) + 2 * n(0, 1) + n(1, 1) - n(-1, -1) - 2 * n(0, -1) - n(1, -1)) / 8.0
    gy = (n(1, -1) + 2 * n(1, 0) + n(1, 1) - n(-1, -1) - 2 * n(-1, 0) - n(-1, 1)) / 8.0
    return gx, gy


def edge_fraction(rgba):
    """Share of pixels that differ from their right or lower neighbour: low for palette atlases and swatches."""
    rgb = rgba[..., :3].astype(np.int32)
    right = np.any(rgb[:, 1:] != rgb[:, :-1], axis=2)
    down = np.any(rgb[1:, :] != rgb[:-1, :], axis=2)
    changed = np.zeros(rgb.shape[:2], dtype=bool)
    changed[:, :-1] |= right
    changed[:-1, :] |= down
    return float(changed.mean())


def make_normal_map(rgba, wrap, target_slope=0.9, min_scale=1.5, max_scale=8.0):
    height = height_from_albedo(rgba, wrap)
    gx, gy = slopes(height, wrap)
    magnitude = np.sqrt(gx * gx + gy * gy)
    p95 = float(np.percentile(magnitude, 95)) if magnitude.size else 0.0
    scale = np.clip(target_slope / max(p95, 1e-6), min_scale, max_scale)
    # Green is up the image (-V): a height rising downwards tilts the normal up the image... towards +green when the
    # height falls going up, i.e. n = (-dh/dx, +dh/dy_down, 1) with dy measured down the rows.
    nx = -gx * scale
    ny = gy * scale
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx / length, ny / length, nz / length], axis=-1)
    encoded = np.clip(np.round((normal * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)
    out = np.dstack([encoded, np.full(encoded.shape[:2], 255, dtype=np.uint8)])
    return out, scale


def new_handle(taken):
    while True:
        handle = random.getrandbits(63) | (1 << 59)
        if handle not in taken:
            taken.add(handle)
            return handle


def write_sidecar(path, handle, settings, newline):
    text = (
        f"Handle: {handle}\n"
        "Type: Texture2D\n"
        "TextureImport:\n"
        "  Format: RGBA\n"
        f"  Filter: {settings['Filter']}\n"
        f"  Wrap: {settings['Wrap']}\n"
        "  SpriteMode: Single\n"
        f"  PixelsPerUnit: {settings['PixelsPerUnit']}\n"
    )
    write_text(path, text, newline)


def set_material_normal(path, handle, strength):
    """Adds NormalHandle / NormalStrength after the albedo (or emissive) handle. False when it has one already."""
    text = read_text(path)
    if re.search(r"^\s*NormalHandle:", text, re.MULTILINE):
        return False
    newline = newline_of(text)
    lines = text.replace("\r\n", "\n").split("\n")
    anchor = None
    for i, line in enumerate(lines):
        if re.match(r"^\s*(AlbedoHandle|EmissiveHandle):", line):
            anchor = i
    if anchor is None:
        return False
    indent = re.match(r"^(\s*)", lines[anchor]).group(1)
    lines[anchor + 1 : anchor + 1] = [f"{indent}NormalHandle: {handle}", f"{indent}NormalStrength: {strength:g}"]
    write_text(path, "\n".join(lines), newline)
    return True


def lit_preview(rgba, normal_rgba):
    """Albedo lit flat, then lit by the normal map from the upper left (a quick look at what the map does)."""
    albedo = rgba[..., :3].astype(np.float64) / 255.0
    n = normal_rgba[..., :3].astype(np.float64) / 255.0 * 2.0 - 1.0
    n[..., 1] = -n[..., 1]  # green up -> image y down for the light below
    light = np.array([-0.55, -0.55, 0.63])  # from the upper left of the image, raised
    light /= np.linalg.norm(light)
    ndl = np.clip(n @ np.array([light[0], light[1], light[2]]), 0.0, 1.0)
    flat = np.clip(albedo * (0.35 + 0.65 * light[2]), 0, 1)
    lit = np.clip(albedo * (0.35 + 0.65 * ndl)[..., None], 0, 1)
    return (np.concatenate([flat, lit], axis=1) * 255).astype(np.uint8)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--scenes", nargs="*", default=None, help=f"scene names (default: {' '.join(DEFAULT_SCENES)})")
    parser.add_argument("--all", action="store_true", help="every material in the project")
    parser.add_argument("--strength", type=float, default=1.0, help="NormalStrength written into the materials")
    parser.add_argument("--min-edges", type=float, default=0.12, help="skip textures with fewer changing pixels than this share")
    parser.add_argument("--force", action="store_true", help="make maps that exist again (their handles are kept)")
    parser.add_argument("--dry-run", action="store_true", help="only list what would be done")
    parser.add_argument("--preview", help="write a before/after sheet of up to 12 textures to this PNG")
    parser.add_argument("--list", help="write the paths written or changed to this file (one per line, relative to the repository)")
    args = parser.parse_args()

    random.seed()
    index = build_handle_index()
    taken = set(index.keys())
    if args.all:
        materials = all_materials()
    else:
        materials = materials_from_scenes(args.scenes if args.scenes else DEFAULT_SCENES, index)
    print(f"{len(materials)} materials")

    # albedo path -> materials using it
    by_albedo = {}
    for material in sorted(materials):
        text = read_text(material)
        albedo = material_field(text, "AlbedoHandle")
        if albedo is None or int(albedo) not in index:
            continue
        path = index[int(albedo)]
        if os.path.splitext(path)[1].lower() not in {".png", ".tga", ".jpg", ".jpeg", ".bmp"}:
            continue
        by_albedo.setdefault(path, []).append(material)

    written = []
    previews = []
    made = skipped = linked = 0
    for albedo, users in sorted(by_albedo.items()):
        rel = os.path.relpath(albedo, ASSETS)
        stem, ext = os.path.splitext(albedo)
        if stem.endswith(SUFFIX):
            continue
        settings = texture_import(albedo)
        if settings["SpriteMode"] != "Single":
            print(f"  skip (sprite sheet) {rel}")
            skipped += 1
            continue
        if "backdrop" in rel.lower() or "sky" in os.path.basename(rel).lower():
            print(f"  skip (backdrop: a painted distance, not a surface) {rel}")
            skipped += 1
            continue
        normal_path = stem + SUFFIX + ".png"
        sidecar = normal_path + ".hymeta"
        rgba = np.array(Image.open(albedo).convert("RGBA"))
        if min(rgba.shape[0], rgba.shape[1]) < 8:
            print(f"  skip (tiny) {rel}")
            skipped += 1
            continue
        edges = edge_fraction(rgba)
        if edges < args.min_edges:
            print(f"  skip (mostly flat colour, {edges:.0%} edges) {rel}")
            skipped += 1
            continue

        handle = None
        if os.path.exists(sidecar):
            match = HANDLE_LINE.search(read_text(sidecar))
            handle = int(match.group(1)) if match else None
        exists = os.path.exists(normal_path) and handle is not None
        if exists is False or args.force:
            normal, scale = make_normal_map(rgba, wrap=settings["Wrap"] == "Repeat")
            print(f"  {'remake' if exists else 'make'} {os.path.relpath(normal_path, ASSETS)} (slope x{scale:.1f}, {len(users)} material(s))")
            if args.dry_run is False:
                Image.fromarray(normal, "RGBA").save(normal_path)
                written.append(normal_path)
                if handle is None:
                    handle = new_handle(taken)
                    newline = newline_of(read_text(albedo + ".hymeta")) if os.path.exists(albedo + ".hymeta") else "\r\n"
                    write_sidecar(sidecar, handle, settings, newline)
                    written.append(sidecar)
            made += 1
            if args.preview and len(previews) < 12:
                previews.append(lit_preview(rgba, normal))
        if handle is None:
            continue
        for material in users:
            if args.dry_run:
                print(f"    would set {os.path.relpath(material, ASSETS)}")
                continue
            if set_material_normal(material, handle, args.strength):
                linked += 1
                written.append(material)

    print(f"{made} normal maps made, {skipped} textures skipped, {linked} materials linked" + (" (dry run)" if args.dry_run else ""))

    if args.preview and previews:
        tile = 160
        rows = []
        for sheet in previews:
            h, w = sheet.shape[:2]
            factor = max(1, tile // h)
            img = Image.fromarray(sheet).resize((w * factor, h * factor), Image.NEAREST)
            rows.append(img)
        width = max(r.width for r in rows)
        canvas = Image.new("RGB", (width, sum(r.height for r in rows) + 4 * len(rows)), (24, 24, 24))
        y = 0
        for r in rows:
            canvas.paste(r, (0, y))
            y += r.height + 4
        canvas.save(args.preview)
        print(f"preview: {args.preview} (left: flat, right: lit by the normal map from the upper left)")

    if args.list:
        with open(args.list, "w", encoding="utf-8") as f:
            for path in written:
                f.write(os.path.relpath(path, ROOT).replace("\\", "/") + "\n")


if __name__ == "__main__":
    sys.exit(main())
