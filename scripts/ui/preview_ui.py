"""Draw a scene's runtime UI offline, the way Scene::ResolveUILayout and SceneRenderer::SubmitUI do, to check a layout
without the engine (a cloud session, a quick look at a builder's output).

    python scripts/ui/preview_ui.py Oxygen/project/Assets/Scenes/Battle.hyscene --out preview.png
        [--size 1920 1080] [--canvas "Battle UI Canvas" ...] [--show "Target Panel" ...] [--hide "Command Panel" ...]
        [--focus "Command 0"] [--world "Enemy 0 Info=700,620" ...] [--background picture.png]

--canvas draws only those canvases (default: every visible one); --show / --hide override an element's or canvas's
Visible; --focus draws that button focused (its tint and its UIFocusVisibilityComponent children); --world places a
World canvas's pivot at view pixels x,y (from the top-left, as image editors count). Not drawn: the camera-projected
World canvases without --world, sprite scenes, anything but the UI. Text uses the same BMFont files, kerning and
wrapping rules (TextLayout), close enough to judge sizes and spacing.

Needs Pillow and PyYAML.
"""

import argparse
import os
import re
import sys

import yaml
from PIL import Image, ImageDraw

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS = os.path.join(ROOT, "Oxygen", "project", "Assets")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("scene")
    parser.add_argument("--out", required=True)
    parser.add_argument("--size", nargs=2, type=int, default=[1920, 1080])
    parser.add_argument("--canvas", action="append", default=[])
    parser.add_argument("--show", action="append", default=[])
    parser.add_argument("--hide", action="append", default=[])
    parser.add_argument("--focus", default=None)
    parser.add_argument("--world", action="append", default=[])
    parser.add_argument("--background", default=None)
    return parser.parse_args()


# ---- Assets -------------------------------------------------------------------------------------------------------

def index_assets():
    handles = {}
    for folder, _, files in os.walk(ASSETS):
        for name in files:
            if name.endswith(".hymeta"):
                path = os.path.join(folder, name)
                with open(path, encoding="utf-8", errors="replace") as file:
                    text = file.read()
                match = re.search(r"Handle:\s*(\d+)", text)
                if match:
                    handles[int(match.group(1))] = (path[: -len(".hymeta")], text)
    return handles


class Textures:
    def __init__(self, handles):
        self.handles = handles
        self.cache = {}

    def get(self, handle):
        if handle in self.cache:
            return self.cache[handle]
        entry = self.handles.get(handle)
        result = None
        if entry is not None and os.path.exists(entry[0]):
            image = Image.open(entry[0]).convert("RGBA")
            meta = yaml.safe_load(entry[1]) or {}
            settings = meta.get("TextureImport", {}) or {}
            cells = None
            if settings.get("SpriteMode") == "Multiple":
                slicing = settings.get("Slicing", {}) or {}
                size = slicing.get("CellSize", [image.width, image.height])
                offset = slicing.get("Offset", [0, 0])
                spacing = slicing.get("Spacing", [0, 0])
                columns = slicing.get("Columns") or max(1, image.width // max(1, size[0]))
                rows = slicing.get("Rows") or max(1, image.height // max(1, size[1]))
                cells = []
                for r in range(rows):
                    for c in range(columns):
                        x = offset[0] + c * (size[0] + spacing[0])
                        y = offset[1] + r * (size[1] + spacing[1])
                        cells.append((x, y, size[0], size[1]))
            nearest = settings.get("Filter", "Nearest") == "Nearest"
            result = (image, cells, nearest)
        self.cache[handle] = result
        return result


class Font:
    def __init__(self, path):
        self.glyphs = {}
        self.kerning = {}
        self.page = None
        folder = os.path.dirname(path)
        with open(path, encoding="utf-8") as file:
            for line in file:
                tag, _, rest = line.strip().partition(" ")
                values = dict(re.findall(r'(\w+)=("[^"]*"|\S+)', rest))
                values = {k: v.strip('"') for k, v in values.items()}
                if tag == "common":
                    self.line_height = int(values["lineHeight"])
                    self.base = int(values["base"])
                elif tag == "page":
                    self.page = Image.open(os.path.join(folder, values["file"])).convert("RGBA")
                elif tag == "char":
                    self.glyphs[int(values["id"])] = {k: int(v) for k, v in values.items()}
                elif tag == "kerning":
                    self.kerning[(int(values["first"]), int(values["second"]))] = int(values["amount"])

    def layout(self, text, scale, width, wrap, align, spacing):
        """Glyph quads (x, y, glyph) from the text's top-left, Y down, as TextLayout::Layout places them."""
        lines = []
        for paragraph in text.split("\n"):
            words = paragraph.split(" ")
            line = ""
            for word in words:
                candidate = word if line == "" else line + " " + word
                if wrap and line != "" and self.measure(candidate, scale) > width:
                    lines.append(line)
                    line = word
                else:
                    line = candidate
            lines.append(line)
        quads = []
        count = 0
        for index, line in enumerate(lines):
            line_width = self.measure(line, scale)
            x = 0.0
            if align == "Center":
                x = (width - line_width) // 2
            elif align == "Right":
                x = width - line_width
            y = round(index * self.line_height * spacing * scale)
            previous = None
            for character in line:
                code = ord(character)
                glyph = self.glyphs.get(code) or self.glyphs.get(ord("?"))
                if previous is not None:
                    x += self.kerning.get((previous, code), 0) * scale
                if glyph is not None:
                    if character not in " ":
                        quads.append((x + glyph["xoffset"] * scale, y + glyph["yoffset"] * scale, glyph, count))
                        count += 1
                    x += glyph["xadvance"] * scale
                previous = code
        height = round(len(lines) * self.line_height * spacing * scale)
        return quads, height

    def measure(self, line, scale):
        x = 0
        previous = None
        for character in line:
            code = ord(character)
            glyph = self.glyphs.get(code) or self.glyphs.get(ord("?"))
            if previous is not None:
                x += self.kerning.get((previous, code), 0)
            if glyph is not None:
                x += glyph["xadvance"]
            previous = code
        return x * scale


# ---- Layout ---------------------------------------------------------------------------------------------------------

def resolve_rect(rect, parent_min, parent_max):
    pmin, pmax = parent_min, parent_max
    size_parent = (pmax[0] - pmin[0], pmax[1] - pmin[1])
    amin = (pmin[0] + rect["AnchorMin"][0] * size_parent[0], pmin[1] + rect["AnchorMin"][1] * size_parent[1])
    amax = (pmin[0] + rect["AnchorMax"][0] * size_parent[0], pmin[1] + rect["AnchorMax"][1] * size_parent[1])
    size = (amax[0] - amin[0] + rect["SizeDelta"][0], amax[1] - amin[1] + rect["SizeDelta"][1])
    pivot = (amin[0] + (amax[0] - amin[0]) * rect["Pivot"][0] + rect["AnchoredPosition"][0],
             amin[1] + (amax[1] - amin[1]) * rect["Pivot"][1] + rect["AnchoredPosition"][1])
    mn = (round(pivot[0] - size[0] * rect["Pivot"][0]), round(pivot[1] - size[1] * rect["Pivot"][1]))
    mx = (max(round(pivot[0] - size[0] * rect["Pivot"][0] + size[0]), mn[0]), max(round(pivot[1] - size[1] * rect["Pivot"][1] + size[1]), mn[1]))
    return mn, mx


def arrange(group, pivot, gmin, gmax, sizes):
    vertical = group.get("Direction", "Vertical") == "Vertical"
    main, cross = (1, 0) if vertical else (0, 1)
    spacing = max(group.get("Spacing", 2), 0)
    pad = group.get("Padding", [0, 0, 0, 0])
    pmin, pmax = (pad[0], pad[1]), (pad[2], pad[3])
    gmin, gmax = list(gmin), list(gmax)
    total = sum(max(s[main], 0) for s in sizes) + (spacing * (len(sizes) - 1) if len(sizes) > 1 else 0)
    if group.get("FitContent", False):
        length = total + pmin[main] + pmax[main]
        position = gmin[main] + pivot[main] * (gmax[main] - gmin[main])
        gmin[main] = round(position - pivot[main] * length)
        gmax[main] = gmin[main] + round(length)
    cmin = [gmin[0] + pmin[0], gmin[1] + pmin[1]]
    cmax = [max(gmax[0] - pmax[0], cmin[0]), max(gmax[1] - pmax[1], cmin[1])]
    slack = cmax[main] - cmin[main] - total
    alignment = group.get("ChildAlignment", "Start")
    cursor = {"Center": slack // 2, "End": slack}.get(alignment, 0)
    out = []
    for s in sizes:
        imin, imax = [0, 0], [0, 0]
        length = max(s[main], 0)
        if vertical:
            imax[main] = cmax[main] - cursor
            imin[main] = imax[main] - length
        else:
            imin[main] = cmin[main] + cursor
            imax[main] = imin[main] + length
        cursor += length + spacing
        if group.get("StretchCrossAxis", True):
            imin[cross], imax[cross] = cmin[cross], cmax[cross]
        else:
            size = max(s[cross], 0)
            room = cmax[cross] - cmin[cross] - size
            offset = {"Center": room // 2, "End": room}.get(alignment, 0)
            imin[cross] = cmin[cross] + offset if vertical else cmax[cross] - offset - size
            imax[cross] = imin[cross] + size
        out.append(((round(imin[0]), round(imin[1])), (round(imax[0]), round(imax[1]))))
    return (gmin[0], gmin[1]), (gmax[0], gmax[1]), out


RECT_DEFAULTS = {"AnchorMin": [0.5, 0.5], "AnchorMax": [0.5, 0.5], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [100, 100], "Visible": True}


def main():
    args = parse_args()
    with open(args.scene, encoding="utf-8") as file:
        scene = yaml.safe_load(file)
    entities = scene["Entities"]
    by_id = {e["Entity"]: e for e in entities}
    children = {}
    for e in entities:
        parent = (e.get("RelationshipComponent") or {}).get("Parent")
        children.setdefault(parent, []).append(e)
    name = lambda e: e["TagComponent"]["Tag"]
    textures = Textures(index_assets())
    fonts = {}
    view_w, view_h = args.size

    image = Image.new("RGBA", (view_w, view_h), (36, 48, 40, 255))
    if args.background:
        back = Image.open(args.background).convert("RGBA").resize((view_w, view_h), Image.LANCZOS)
        image.alpha_composite(back)

    world = {}
    for item in args.world:
        key, _, value = item.partition("=")
        x, y = value.split(",")
        world[key] = (float(x), view_h - float(y))

    def visible_of(e, own):
        if name(e) in args.show:
            return True
        if name(e) in args.hide:
            return False
        return own

    canvases = [e for e in entities if "UICanvasComponent" in e]
    canvases = [c for c in canvases if (args.canvas == [] and visible_of(c, c["UICanvasComponent"].get("Visible", True))) or name(c) in args.canvas]
    canvases.sort(key=lambda c: c["UICanvasComponent"].get("SortOrder", 0))

    for canvas_entity in canvases:
        canvas = canvas_entity["UICanvasComponent"]
        ref = canvas.get("ReferenceResolution", [1920, 1080])
        mode = canvas.get("ScaleMode", "ScaleWithScreen")
        fit = min(view_w / ref[0], view_h / ref[1])
        if mode == "ScaleWithScreen":
            scale = fit
            size = (view_w / scale, view_h / scale)
        elif mode == "ConstantPixelSize":
            scale, size = 1.0, (view_w, view_h)
        else:
            scale = max(int(fit), 1)
            size = (max(view_w // scale, ref[0]), max(view_h // scale, ref[1]))
        offset = ((view_w - size[0] * scale) // 2, (view_h - size[1] * scale) // 2)
        if canvas.get("Space") == "World":
            size = tuple(canvas.get("WorldSize", [320, 80]))
            pivot = canvas.get("WorldPivot", [0.5, 0])
            if name(canvas_entity) not in world:
                print("skipped World canvas '%s' (place it with --world)" % name(canvas_entity))
                continue
            p = world[name(canvas_entity)]
            offset = (round(p[0] - pivot[0] * size[0] * scale), round(p[1] - pivot[1] * size[1] * scale))

        def to_view(point):
            x = round(offset[0] + point[0] * scale)
            y = round(offset[1] + point[1] * scale)
            return x, view_h - y  # image rows run down

        resolved = {}
        driven = {}

        def walk(e, pmin, pmax, visible):
            rect = e.get("UIRectComponent")
            if rect is not None:
                rect = dict(RECT_DEFAULTS, **rect)
                if e["Entity"] in driven:
                    mn, mx = driven[e["Entity"]]
                else:
                    mn, mx = resolve_rect(rect, pmin, pmax)
                visible = visible and visible_of(e, rect.get("Visible", True))
                group = e.get("UILayoutGroupComponent")
                if group is not None:
                    kids = [k for k in children.get(e["Entity"], []) if "UIRectComponent" in k and "UICanvasComponent" not in k
                            and visible_of(k, k["UIRectComponent"].get("Visible", True))]
                    sizes = []
                    for k in kids:
                        kmin, kmax = resolve_rect(dict(RECT_DEFAULTS, **k["UIRectComponent"]), mn, mx)
                        sizes.append((kmax[0] - kmin[0], kmax[1] - kmin[1]))
                    mn, mx, places = arrange(group, rect["Pivot"], mn, mx, sizes)
                    for k, place in zip(kids, places):
                        driven[k["Entity"]] = place
                resolved[e["Entity"]] = [mn, mx, visible, [(-1e9, -1e9), (1e9, 1e9)]]
                pmin, pmax = mn, mx
            for k in children.get(e["Entity"], []):
                if "UICanvasComponent" not in k:
                    walk(k, pmin, pmax, visible)

        walk(canvas_entity, (0, 0), size, True)

        # Scroll views (the offline preview starts at the authored position and keeps --focus in view).
        def subtree(e):
            for k in children.get(e["Entity"], []):
                yield k
                yield from subtree(k)

        for e in [canvas_entity] + list(subtree(canvas_entity)):
            scroll = e.get("UIScrollViewComponent")
            if scroll is None or e["Entity"] not in resolved:
                continue
            vmin, vmax = resolved[e["Entity"]][0], resolved[e["Entity"]][1]
            bottoms = [resolved[k["Entity"]][0][1] for k in children.get(e["Entity"], []) if k["Entity"] in resolved and resolved[k["Entity"]][2]]
            max_scroll = max(0, vmin[1] - min(bottoms)) if bottoms else 0
            position = scroll.get("ScrollPosition", 0)
            for k in subtree(e):
                if name(k) == args.focus and k["Entity"] in resolved:
                    fmin, fmax = resolved[k["Entity"]][0], resolved[k["Entity"]][1]
                    if fmax[1] + position > vmax[1]:
                        position = vmax[1] - fmax[1]
                    if fmin[1] + position < vmin[1]:
                        position = vmin[1] - fmin[1]
            position = round(min(max(position, 0), max_scroll))
            for k in subtree(e):
                if k["Entity"] in resolved:
                    r = resolved[k["Entity"]]
                    r[0] = (r[0][0], r[0][1] + position)
                    r[1] = (r[1][0], r[1][1] + position)
                    r[3] = ((max(r[3][0][0], vmin[0]), max(r[3][0][1], vmin[1])), (min(r[3][1][0], vmax[0]), min(r[3][1][1], vmax[1])))
            thumb_id = scroll.get("Scrollbar")
            if thumb_id in resolved:
                thumb = by_id[thumb_id]
                track = resolved.get((thumb.get("RelationshipComponent") or {}).get("Parent"))
                if max_scroll <= 0 or track is None:
                    resolved[thumb_id][2] = False
                else:
                    th = track[1][1] - track[0][1]
                    vh = vmax[1] - vmin[1]
                    height = round(min(max(th * vh / max(vh + max_scroll, 1), min(8, th)), th))
                    top = track[1][1] - round((th - height) * (position / max_scroll))
                    resolved[thumb_id][0] = (resolved[thumb_id][0][0], top - height)
                    resolved[thumb_id][1] = (resolved[thumb_id][1][0], top)

        layer = Image.new("RGBA", (view_w, view_h), (0, 0, 0, 0))

        def blit(source, box, colour, clip, nearest):
            """Draw `source` (an RGBA image) stretched over canvas box (min, max), tinted, cut to clip."""
            (x0, y0), (x1, y1) = box
            (cx0, cy0), (cx1, cy1) = clip
            ix0, iy0 = max(x0, cx0), max(y0, cy0)
            ix1, iy1 = min(x1, cx1), min(y1, cy1)
            if ix1 <= ix0 or iy1 <= iy0:
                return
            left, top = to_view((ix0, iy1))
            right, bottom = to_view((ix1, iy0))
            if right <= left or bottom <= top:
                return
            if source is None:
                patch = Image.new("RGBA", (right - left, bottom - top), tuple(int(c * 255) for c in colour))
            else:
                sw, sh = source.size
                fx0 = (ix0 - x0) / (x1 - x0)
                fx1 = (ix1 - x0) / (x1 - x0)
                fy0 = (y1 - iy1) / (y1 - y0)
                fy1 = (y1 - iy0) / (y1 - y0)
                crop = source.crop((round(fx0 * sw), round(fy0 * sh), max(round(fx1 * sw), round(fx0 * sw) + 1), max(round(fy1 * sh), round(fy0 * sh) + 1)))
                patch = crop.resize((right - left, bottom - top), Image.NEAREST if nearest else Image.BILINEAR)
                if colour != (1, 1, 1, 1):
                    r, g, b, a = patch.split()
                    r = r.point(lambda v: v * colour[0])
                    g = g.point(lambda v: v * colour[1])
                    b = b.point(lambda v: v * colour[2])
                    a = a.point(lambda v: v * colour[3])
                    patch = Image.merge("RGBA", (r, g, b, a))
            layer.alpha_composite(patch, (left, top))

        def mul(a, b):
            return tuple(x * y for x, y in zip(a, b))

        def draw_element(e, tint, focused):
            if e["Entity"] not in resolved:
                return
            mn, mx, visible, clip = resolved[e["Entity"]]
            if visible is False:
                return
            img = e.get("UIImageComponent")
            if img is not None:
                colour = mul(tuple(img.get("Color", [1, 1, 1, 1])), tint)
                tex = textures.get(img.get("TextureHandle", 0)) if img.get("TextureHandle") else None
                if tex is None:
                    blit(None, (mn, mx), colour, clip, True)
                else:
                    source, cells, nearest = tex
                    if cells is not None:
                        x, y, w, h = cells[min(img.get("SpriteIndex", 0), len(cells) - 1)]
                        source = source.crop((x, y, x + w, y + h))
                    if img.get("FlipX"):
                        source = source.transpose(Image.FLIP_LEFT_RIGHT)
                    if img.get("FlipY"):
                        source = source.transpose(Image.FLIP_TOP_BOTTOM)
                    l, b, r, t = img.get("NineSlice", [0, 0, 0, 0])
                    if (l, b, r, t) == (0, 0, 0, 0):
                        blit(source, (mn, mx), colour, clip, nearest)
                    else:
                        sw, sh = source.size
                        w, h = mx[0] - mn[0], mx[1] - mn[1]
                        dl, dr = min(l, w / 2), min(r, w / 2)
                        db, dt = min(b, h / 2), min(t, h / 2)
                        xs = [mn[0], mn[0] + dl, mx[0] - dr, mx[0]]
                        ys = [mn[1], mn[1] + db, mx[1] - dt, mx[1]]
                        sx = [0, l, sw - r, sw]
                        sy = [sh, sh - b, t, 0]  # bottom to top in source rows
                        for row in range(3):
                            for col in range(3):
                                box = ((xs[col], ys[row]), (xs[col + 1], ys[row + 1]))
                                if box[1][0] <= box[0][0] or box[1][1] <= box[0][1]:
                                    continue
                                cell = source.crop((sx[col], sy[row + 1], max(sx[col + 1], sx[col] + 1), max(sy[row], sy[row + 1] + 1)))
                                blit(cell, box, colour, clip, nearest)
            text = e.get("UITextComponent")
            if text is not None and text.get("Text") and text.get("FontHandle"):
                entry = textures.handles.get(text["FontHandle"])
                if entry is None:
                    return
                font = fonts.get(entry[0]) or Font(entry[0])
                fonts[entry[0]] = font
                scale_text = text.get("Scale", 1)
                quads, height = font.layout(str(text["Text"]), scale_text, mx[0] - mn[0], text.get("Wrap", True), text.get("AlignH", "Left"), text.get("LineSpacing", 1))
                valign = text.get("AlignV", "Top")
                top = mx[1]
                if valign == "Middle":
                    top = mx[1] - (mx[1] - mn[1] - height) // 2
                elif valign == "Bottom":
                    top = mn[1] + height
                visible_count = text.get("VisibleCharacters", -1)
                passes = []
                shadow = text.get("ShadowColor", [0, 0, 0, 0])
                if shadow[3] > 0:
                    passes.append((tuple(text.get("ShadowOffset", [2, -2])), (shadow[0], shadow[1], shadow[2], shadow[3] * tint[3] * text.get("Color", [1, 1, 1, 1])[3])))
                passes.append(((0, 0), mul(tuple(text.get("Color", [1, 1, 1, 1])), tint)))
                for (ox, oy), colour in passes:
                    for x, y, glyph, index in quads:
                        if 0 <= visible_count <= index:
                            continue
                        source = font.page.crop((glyph["x"], glyph["y"], glyph["x"] + max(glyph["width"], 1), glyph["y"] + max(glyph["height"], 1)))
                        gx0 = mn[0] + x + ox
                        gy1 = top - y + oy
                        box = ((gx0, gy1 - glyph["height"] * scale_text), (gx0 + glyph["width"] * scale_text, gy1))
                        blit(source, box, colour, clip, False)

        def draw_tree(e, tint, focused, depth_skip=False):
            if "UIButtonComponent" in e:
                button = e["UIButtonComponent"]
                if button.get("Interactable", True) is False:
                    tint = mul(tint, tuple(button.get("DisabledTint", [1, 1, 1, 0.4])))
                    focused = False
                elif name(e) == args.focus:
                    tint = mul(tint, tuple(button.get("FocusedTint", [1, 0.95, 0.75, 1])))
                    focused = True
                else:
                    tint = mul(tint, tuple(button.get("NormalTint", [1, 1, 1, 1])))
                    focused = False
            visibility = e.get("UIFocusVisibilityComponent")
            if visibility is not None and (visibility.get("Show", "Focused") == "Focused") != focused:
                return
            draw_element(e, tint, focused)
            for k in children.get(e["Entity"], []):
                if "UICanvasComponent" not in k:
                    draw_tree(k, tint, focused)

        draw_tree(canvas_entity, (1, 1, 1, 1), False)
        image.alpha_composite(layer)

    image.convert("RGB").save(args.out)
    print("wrote", args.out)


if __name__ == "__main__":
    sys.exit(main())
