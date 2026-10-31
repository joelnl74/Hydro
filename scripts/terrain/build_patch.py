"""Builds the TR0 test patch for one or more presets: the mesh (by material), the baked top texture and the
fringe strip, into scripts/terrain/out/<preset>/. Render them with preview_blender.py.

    python scripts/terrain/build_patch.py [preset ...]      (default: bare conservative stronger tr1)

Needs numpy and Pillow (not Blender)."""

import json
import os
import sys
import time

import terrain_mesher as tm
from splat import SplatSettings, bake, make_fringe, winner_lookup
from test_patch import CAPS, LAYERS, PRESETS, SIZE_M, WALLS, build_terrain, build_weights, preset_layers_and_splat

HERE = os.path.dirname(os.path.abspath(__file__))


def build(preset, weights, seed=1234):
    settings = PRESETS[preset]
    layers, splat_settings = preset_layers_and_splat(preset)
    out = os.path.join(HERE, "out", preset)
    os.makedirs(out, exist_ok=True)
    terrain = build_terrain(settings, seed)
    top_image, win = bake(weights, layers, seed, splat_settings, SIZE_M, SIZE_M)
    top_image.save(os.path.join(out, "top.png"))
    materials = {
        "top": {"texture": "top.png", "tint": [1, 1, 1]},
        "caps": {"texture": CAPS["texture"], "tint": list(CAPS["tint"])},
    }
    if settings.fringe_px > 0:
        make_fringe(LAYERS[0].texture, settings.fringe_px, seed).save(os.path.join(out, "fringe.png"))
        materials["fringe"] = {"texture": "fringe.png", "tint": [1, 1, 1], "alpha": True}
    wall_tiling = {}
    for k, wall in WALLS.items():
        wall_tiling[k] = wall["tiling"]
        materials[tm.wall_part_name(k)] = {"texture": wall["texture"], "tint": list(wall["tint"])}
        # The lip's front face: the wall's material, a little darker (the plan's open point, decided here).
        materials["lip_face_%d" % k] = {"texture": wall["texture"], "tint": [c * 0.8 for c in wall["tint"]]}
    mb, info = tm.mesh_terrain(terrain, top_layer_at=winner_lookup(win), fringe_layers=(0,), wall_tiling=wall_tiling)
    parts = {}
    for name, part in mb.parts.items():
        parts[name] = {
            "positions": [round(v, 5) for p in part["positions"] for v in p],
            "uvs": [round(v, 5) for uv in part["uvs"] for v in uv],
        }
    data = {
        "preset": preset,
        "size": SIZE_M,
        "settings": settings.__dict__,
        "splat": {"roughness": [l.roughness for l in layers], "noise_cell_px": splat_settings.noise_cell_px, "pixel_noise": splat_settings.pixel_noise},
        "materials": materials,
        "parts": parts,
        "info": info,
    }
    with open(os.path.join(out, "patch.json"), "w") as f:
        json.dump(data, f)
    return info


def main(argv):
    presets = argv or ["bare", "conservative", "stronger", "tr1"]
    splat_settings = SplatSettings()
    t = time.time()
    weights = build_weights(splat_settings)
    print("weights %.1fs" % (time.time() - t))
    for preset in presets:
        t = time.time()
        info = build(preset, weights)
        print("%-13s %5d triangles, %3d wall segments, %.1fs%s" % (preset, info["triangles"], info["segments"], time.time() - t,
                                                                   ("  problems: %s" % info["problems"]) if info["problems"] else ""))


if __name__ == "__main__":
    main(sys.argv[1:])
