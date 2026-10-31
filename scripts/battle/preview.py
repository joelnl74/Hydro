#!/usr/bin/env python3
"""A rough offline preview of Battle.hyscene through the battle's stage camera (not the engine's renderer).

    python scripts/battle/preview.py [out.png]

It ray-casts the flat floor meshes and the backdrop with their materials' textures, stands the sprites up as
billboards (sorted far to near), draws the GLB props as simple stand-ins, darkens everything towards the scene's
fog colour, adds warm pools for the point lights, then draws the HUD (Overlay sprites) on top. Good for checking
the layout and the framing; the lighting, shadows and bloom are only hinted at. Needs numpy, Pillow, PyYAML.
"""
from __future__ import annotations

import math
import sys
from functools import lru_cache
from pathlib import Path

import numpy as np
import yaml
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stage import StageCamera  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "Oxygen" / "project" / "Assets"
WIDTH, HEIGHT = 1280, 720

HANDLES: dict[int, Path] = {}


def index_handles():
    if HANDLES:
        return
    for path in ASSETS.rglob("*"):
        if path.is_file() and path.suffix in (".hymeta", ".hymat", ".hyanim", ".hyrender"):
            try:
                with path.open(encoding="utf-8") as handle_file:
                    for line in handle_file:
                        if line.startswith("Handle:"):
                            HANDLES[int(line.split(":", 1)[1])] = path.with_suffix("") if path.suffix == ".hymeta" else path
                            break
            except (UnicodeDecodeError, ValueError):
                pass


@lru_cache(maxsize=None)
def load_image(path: Path):
    return Image.open(path).convert("RGBA")


@lru_cache(maxsize=None)
def sprite_rects(texture: Path):
    meta_path = Path(str(texture) + ".hymeta")
    meta = yaml.safe_load(meta_path.read_text(encoding="utf-8")) if meta_path.exists() else {}
    config = (meta or {}).get("TextureImport", {})
    image = load_image(texture)
    rects, names = [], {}
    if config.get("SpriteMode") == "Multiple":
        slicing = config.get("Slicing", {})
        sprites = config.get("Sprites")
        if sprites:
            for i, s in enumerate(sprites):
                rects.append(tuple(s["Rect"]))
                names[s["Name"]] = i
        else:
            cw, ch = slicing.get("CellSize", [image.width, image.height])
            columns = slicing.get("Columns") or image.width // cw
            rows = slicing.get("Rows") or image.height // ch
            row_names = slicing.get("RowNames") or []
            for r in range(rows):
                for c in range(columns):
                    names[f"{row_names[r] if r < len(row_names) else 'Sprite'}_{c}"] = len(rects)
                    rects.append((c * cw, r * ch, cw, ch))
    if not rects:
        rects = [(0, 0, image.width, image.height)]
    return rects, names


def sprite_image(sprite: dict):
    handle = sprite.get("TextureHandle", 0)
    path = HANDLES.get(handle)
    if not handle or path is None or not path.exists():
        return None
    image = load_image(path)
    rects, names = sprite_rects(path)
    index = names.get(sprite.get("Sprite"), sprite.get("SpriteIndex", 0))
    x, y, w, h = rects[min(index, len(rects) - 1)]
    cell = image.crop((x, y, x + w, y + h))
    if sprite.get("FlipX"):
        cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
    return cell


def sprite_pivot_y(sprite: dict) -> float:
    """The sheet's pivot height (0.5 = the cell's centre); the engine moves the quad so it is at the entity."""
    path = HANDLES.get(sprite.get("TextureHandle", 0))
    if path is None:
        return 0.5
    meta_path = Path(str(path) + ".hymeta")
    meta = yaml.safe_load(meta_path.read_text(encoding="utf-8")) if meta_path.exists() else {}
    config = (meta or {}).get("TextureImport", {})
    if config.get("SpriteMode") != "Multiple":
        return 0.5
    return float(config.get("Slicing", {}).get("DefaultPivot", [0.5, 0.5])[1])


def material_texture(material_handle: int):
    path = HANDLES.get(material_handle)
    if path is None:
        return None, (1.0, 1.0), (1, 1, 1, 1)
    material = yaml.safe_load(path.read_text(encoding="utf-8"))["Material"]
    texture = HANDLES.get(material.get("AlbedoHandle", 0))
    image = np.asarray(load_image(texture)).astype(np.float32) / 255.0 if texture and texture.exists() else None
    return image, tuple(material.get("Tiling", [1, 1])), tuple(material.get("Tint", [1, 1, 1, 1]))


class Projector:
    def __init__(self, camera: StageCamera):
        self.c = camera
        self.f = (HEIGHT / 2) / math.tan(math.radians(camera.field_of_view) / 2)

    def project(self, p):
        v = np.asarray(p, dtype=float) - self.c.position
        z = float(np.dot(v, self.c.forward))
        if z <= 0.1:
            return None
        x = float(np.dot(v, self.c.right))
        y = float(np.dot(v, self.c.up))
        return (WIDTH / 2 + x / z * self.f, HEIGHT / 2 - y / z * self.f, z)


def to_world(entities):
    """Children's Translation made world: the builder's folders only translate (no rotation or scale)."""
    by_uuid = {e["Entity"]: e for e in entities}

    def offset(entity):
        parent = by_uuid.get(entity.get("RelationshipComponent", {}).get("Parent"))
        if parent is None:
            return np.zeros(3)
        return offset(parent) + np.array(parent.get("TransformComponent", {}).get("Translation", [0, 0, 0]), float)

    offsets = {e["Entity"]: offset(e) for e in entities}
    for e in entities:
        t = e.get("TransformComponent")
        if t is not None:
            t["Translation"] = list(np.array(t["Translation"], float) + offsets[e["Entity"]])
    return entities


def render(scene_path: Path, out: Path, camera: StageCamera):
    index_handles()
    scene = yaml.safe_load(Path(scene_path).read_text(encoding="utf-8"))
    entities = to_world(scene["Entities"])
    proj = Projector(camera)
    env = scene.get("Environment", {})
    profile_path = HANDLES.get(scene.get("Look", {}).get("ProfileHandle", 0))
    if profile_path is not None:
        env = yaml.safe_load(profile_path.read_text(encoding="utf-8"))["RenderProfile"].get("Environment", env)
    fog_colour = np.array(env.get("FogColor", [0.6, 0.45, 0.4]))
    fog_start, fog_end = env.get("FogStart", 13), env.get("FogEnd", 38)
    light_tint = np.array([0.82, 0.7, 0.62])

    # Rays through every pixel.
    ys, xs = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.float32)
    dirs = (camera.forward[None, None, :] * proj.f
            + camera.right[None, None, :] * (xs - WIDTH / 2)[..., None]
            - camera.up[None, None, :] * (ys - HEIGHT / 2)[..., None])
    dirs /= np.linalg.norm(dirs, axis=2, keepdims=True)
    colour = np.zeros((HEIGHT, WIDTH, 3), np.float32)
    colour[:] = fog_colour * 0.9
    depth = np.full((HEIGHT, WIDTH), np.inf, np.float32)

    def shade(rgb, dist):
        fog = np.clip((dist - fog_start) / max(fog_end - fog_start, 1e-3), 0, 1)[..., None] * 0.85
        return rgb * light_tint * (1 - fog) + fog_colour * fog

    # Floor and backdrop meshes (built-in shapes only).
    meshes = [e for e in entities if "MeshComponent" in e and "ModelHandle" not in e["MeshComponent"]]
    meshes.sort(key=lambda e: e["TransformComponent"]["Translation"][2] + (e["MeshComponent"]["Size"][2] * 0.5 if e["MeshComponent"]["Shape"] == "Box" else 0))
    for e in meshes:
        m = e["MeshComponent"]
        t = np.array(e["TransformComponent"]["Translation"], float)
        size = np.array(m["Size"], float)
        material = m.get("Materials", [{}])[0]
        image, tiling, tint = material_texture(material.get("MaterialHandle", 0))
        inline_tint = material.get("Inline", {}).get("Tint", [1, 1, 1, 1])
        vertical = m["Shape"] == "Box" and size[2] > size[1] * 4
        if vertical:
            plane_y = t[1] - size[1] / 2
            tt = (plane_y - camera.position[1]) / np.where(np.abs(dirs[..., 1]) < 1e-6, 1e-6, dirs[..., 1])
            hit = camera.position[None, None, :] + dirs * tt[..., None]
            u = (hit[..., 0] - (t[0] - size[0] / 2)) / size[0]
            v = 1 - (hit[..., 2] - (t[2] - size[2] / 2)) / size[2]
        else:
            top = t[2] + (size[2] / 2 if m["Shape"] == "Box" else 0)
            tt = (top - camera.position[2]) / np.where(np.abs(dirs[..., 2]) < 1e-6, -1e-6, dirs[..., 2])
            hit = camera.position[None, None, :] + dirs * tt[..., None]
            u = (hit[..., 0] - t[0]) * tiling[0]
            v = (hit[..., 1] - t[1]) * tiling[1]
        inside = (tt > 0) & (tt < depth + 0.05)
        if vertical:
            inside &= (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
        else:
            inside &= (np.abs(hit[..., 0] - t[0]) <= size[0] / 2) & (np.abs(hit[..., 1] - t[1]) <= size[1] / 2)
        if image is not None:
            h, w = image.shape[:2]
            px = (np.mod(u, 1.0) * (w - 1)).astype(int) if not vertical else (np.clip(u, 0, 1) * (w - 1)).astype(int)
            py = (np.mod(-v, 1.0) * (h - 1)).astype(int) if not vertical else (np.clip(v, 0, 1) * (h - 1)).astype(int)
            rgb = image[py, px, :3]
        else:
            rgb = np.ones((HEIGHT, WIDTH, 3), np.float32) * 0.5
        rgb = rgb * np.array(tint[:3]) * np.array(inline_tint[:3])
        lit = shade(rgb, tt) if not vertical else rgb * 0.95
        colour[inside] = lit[inside]
        depth[inside] = tt[inside]

    canvas = Image.fromarray((np.clip(colour, 0, 1) * 255).astype(np.uint8)).convert("RGBA")

    # Warm pools under point lights (lanterns).
    glow = np.zeros((HEIGHT, WIDTH, 3), np.float32)
    for e in entities:
        light = e.get("Light2DComponent")
        if not light or light.get("Type") != "Point" or light.get("Intensity", 0) <= 0:
            continue
        p = np.array(e["TransformComponent"]["Translation"], float) + [0, 0, light.get("Height", 0)]
        s = proj.project(p)
        if s is None:
            continue
        radius = light.get("Radius", 3) * proj.f / s[2]
        d2 = ((xs - s[0]) ** 2 + ((ys - s[1]) * 1.6) ** 2) / max(radius, 1) ** 2
        glow += np.exp(-d2 * 2.5)[..., None] * np.array(light.get("Color", [1, 0.6, 0.3])) * 0.28 * light.get("Intensity", 1)
    canvas = Image.fromarray(np.clip(np.asarray(canvas)[..., :3] / 255.0 + glow, 0, 1).__mul__(255).astype(np.uint8)).convert("RGBA")
    draw = ImageDraw.Draw(canvas)

    # Contact shadows: dark soft ellipses on the floor.
    for e in entities:
        shadow, t = e.get("Shadow2DComponent"), e.get("TransformComponent")
        if shadow is None or t is None or shadow.get("Strength", 0) <= 0.01:
            continue
        c = proj.project(t["Translation"])
        if c is None:
            continue
        size = shadow.get("Size", [1, 0.3])
        a = proj.project(np.array(t["Translation"], float) + [size[0] / 2, size[1] / 2, 0])
        if a is None:
            continue
        rx, ry = abs(a[0] - c[0]), max(abs(a[1] - c[1]), 2)
        layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).ellipse([c[0] - rx, c[1] - ry, c[0] + rx, c[1] + ry], fill=(0, 0, 0, int(255 * shadow["Strength"] * 0.8)))
        canvas.alpha_composite(layer)
    draw = ImageDraw.Draw(canvas)

    # Standing things, far to near: sprites as billboards, GLB props as stand-ins.
    items = []
    for e in entities:
        t = e.get("TransformComponent")
        if t is None:
            continue
        sprite = e.get("SpriteRendererComponent")
        if sprite and not sprite.get("Overlay") and sprite.get("Facing") in ("Upright", "Billboard"):
            if sprite.get("Color", [1, 1, 1, 1])[3] <= 0.01:
                continue
            anchor = np.array(t["Translation"], float)
            if sprite.get("Facing") == "Upright":
                # The quad's bottom edge, where it stands: back from the entity by the pivot.
                anchor = anchor - np.array([0.0, sprite_pivot_y(sprite) * abs(t["Scale"][1]), 0.0])
            s = proj.project(anchor)
            if s:
                items.append((s[2], "sprite", e, s))
        mesh = e.get("MeshComponent")
        if mesh and "ModelHandle" in mesh:
            s = proj.project(t["Translation"])
            if s:
                items.append((s[2], "model", e, s))
    items.sort(key=lambda i: -i[0])
    for z, kind, e, s in items:
        t = e["TransformComponent"]
        if kind == "sprite":
            sprite = e["SpriteRendererComponent"]
            cell = sprite_image(sprite)
            w = max(1, int(abs(t["Scale"][0]) * proj.f / z))
            h = max(1, int(abs(t["Scale"][1]) * proj.f / z))
            if cell is None or w > 4000 or h > 4000:
                continue
            cell = cell.resize((w, h), Image.NEAREST)
            arr = np.asarray(cell).astype(np.float32) / 255.0
            tint = np.array(sprite.get("Color", [1, 1, 1, 1]))
            fog = min(max((z - fog_start) / max(fog_end - fog_start, 1e-3), 0), 1) * 0.85
            rgb = arr[..., :3] * tint[:3] * light_tint * (1 - fog) + fog_colour * fog
            arr = np.dstack([rgb, arr[..., 3] * tint[3]])
            cell = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
            if sprite.get("Facing") == "Upright":
                canvas.alpha_composite(cell, (int(s[0] - w / 2), int(s[1] - h)))
            else:
                canvas.alpha_composite(cell, (int(s[0] - w / 2), int(s[1] - h / 2)))
        else:
            name = str(HANDLES.get(e["MeshComponent"]["ModelHandle"], ""))
            base = np.array(t["Translation"], float)
            scale = t["Scale"][0]
            if "Torii" in name:
                red = (150, 40, 32, 255)
                for x in (-1.9, 1.9):
                    a, b = proj.project(base + [x * scale - 0.2, 0, 0]), proj.project(base + [x * scale + 0.2, 0, 4.8 * scale])
                    if a and b:
                        draw.rectangle([a[0], b[1], b[0], a[1]], fill=red)
                for zt, half, thick in ((4.6, 2.7, 0.35), (3.7, 2.3, 0.22)):
                    a, b = proj.project(base + [-half * scale, 0, zt * scale]), proj.project(base + [half * scale, 0, (zt + thick) * scale])
                    if a and b:
                        draw.rectangle([a[0], b[1], b[0], a[1]], fill=(60, 52, 58, 255) if zt > 4 else red)
            else:
                grey = (96, 94, 88, 255)
                a, b = proj.project(base + [-0.3 * scale, 0, 0]), proj.project(base + [0.3 * scale, 0, 1.6 * scale])
                if a and b:
                    draw.rectangle([a[0], b[1], b[0], a[1]], fill=grey)
                    c = proj.project(base + [0, 0, 1.33 * scale])
                    if c:
                        draw.rectangle([c[0] - 4, c[1] - 4, c[0] + 4, c[1] + 4], fill=(255, 190, 110, 255))

    # HUD on top, in SortOrder.
    hud = []
    for e in entities:
        sprite = e.get("SpriteRendererComponent")
        if sprite and sprite.get("Overlay"):
            hud.append((sprite.get("SortOrder", 0), e))
    hud.sort(key=lambda i: i[0])
    for _, e in hud:
        sprite = e["SpriteRendererComponent"]
        tag = e["TagComponent"]["Tag"]
        colour4 = sprite.get("Color", [1, 1, 1, 1])
        if colour4[3] <= 0.01 or tag in ("Battle Overlay",) or "Popup" in tag or tag in ("Target Cursor", "Turn Marker"):
            continue
        t = e["TransformComponent"]
        s = proj.project(t["Translation"])
        if not s:
            continue
        w = max(1, int(abs(t["Scale"][0]) * proj.f / s[2]))
        h = max(1, int(abs(t["Scale"][1]) * proj.f / s[2]))
        cell = sprite_image(sprite)
        if cell is None:
            cell = Image.new("RGBA", (4, 4), (255, 255, 255, 255))
        cell = cell.resize((w, h), Image.NEAREST)
        arr = np.asarray(cell).astype(np.float32) / 255.0 * np.array(colour4)
        canvas.alpha_composite(Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8)), (int(s[0] - w / 2), int(s[1] - h / 2)))

    canvas.convert("RGB").save(out)
    print(f"preview: {out}")


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from build_arena import CAMERA  # noqa: E402

    render(ASSETS / "Scenes" / "Battle.hyscene", Path(sys.argv[1] if len(sys.argv) > 1 else "battle_preview.png"), CAMERA)
