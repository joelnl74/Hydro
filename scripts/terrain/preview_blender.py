"""Renders the TR0 test patch (built by build_patch.py) at the Farm Village's game camera.

    blender --background --python scripts/terrain/preview_blender.py -- [preset ...] [--views game,close,west]
    python scripts/terrain/preview_blender.py [preset ...]          (with the pip 'bpy' module)

Writes scripts/terrain/out/<preset>/<view>.png. Camera: the Farm Village's framing (55 degrees from straight
down, 30 degree vertical field of view, 12 m visible at the focus). Light: its sun and the Golden render
profile's ambient, forward-lit like Hydro (direct light only, no bounces). Textures nearest-filtered."""

import json
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))

TILT_DEGREES = 55.0
FOV_DEGREES = 30.0
SUN_ROTATION = (1.01229095, 0.0, -0.663224995)   # FarmVillage.hyscene's Sun
SUN_COLOUR = (1.0, 0.8, 0.56)
SKY_AMBIENT = (0.42, 0.36, 0.30)                  # RenderProfiles/FarmVillage/Golden.hyrender
EXPOSURE = 0.1

# name: (focus x, y, z, metres visible vertically at the focus)
VIEWS = {
    "game": (10.0, 9.6, 0.4, 12.0),
    "close": (8.0, 12.6, 0.5, 5.0),
    "west": (3.6, 8.2, 0.2, 5.0),
}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        import addon_utils
        addon_utils.enable("cycles")
    except Exception:
        pass


def image(path, base, alpha=False):
    full = path if os.path.isabs(path) else os.path.join(base, path)
    img = bpy.data.images.load(full, check_existing=True)
    img.alpha_mode = "STRAIGHT" if alpha else "NONE"
    return img


def material(name, spec, base):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = image(spec["texture"], base, spec.get("alpha", False))
    tex.interpolation = "Closest"
    tex.extension = "EXTEND" if name == "top" else "REPEAT"
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    links.new(tex.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (*spec.get("tint", [1, 1, 1]), 1.0)
    links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    if spec.get("alpha", False):
        links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    return mat


def add_part(name, part, mat):
    pos = part["positions"]
    uvs = part["uvs"]
    verts = [(pos[k], pos[k + 1], pos[k + 2]) for k in range(0, len(pos), 3)]
    faces = [(k, k + 1, k + 2) for k in range(0, len(verts), 3)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    layer = mesh.uv_layers.new(name="UV")
    for loop in mesh.loops:
        k = loop.vertex_index
        layer.data[loop.index].uv = (uvs[2 * k], uvs[2 * k + 1])
    mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def setup_scene(resolution):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 96
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = 2
    scene.cycles.diffuse_bounces = 0       # forward lighting: direct sun + ambient only
    scene.cycles.glossy_bounces = 0
    scene.cycles.transparent_max_bounces = 8
    scene.cycles.filter_width = 0.6        # keep art pixels crisp
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = EXPOSURE

    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (*SKY_AMBIENT, 1.0)
    scene.world = world

    sun = bpy.data.lights.new("Sun", "SUN")
    sun.color = SUN_COLOUR
    sun.energy = math.pi            # NdotL = 1 lights a surface by exactly its albedo, as in Hydro
    sun.angle = math.radians(0.5)   # crisp shadows
    obj = bpy.data.objects.new("Sun", sun)
    obj.rotation_euler = SUN_ROTATION
    scene.collection.objects.link(obj)

    cam = bpy.data.cameras.new("Camera")
    cam.sensor_fit = "VERTICAL"
    cam.angle_y = math.radians(FOV_DEGREES)
    cam.clip_start = 0.5
    cam.clip_end = 300.0
    cam_obj = bpy.data.objects.new("Camera", cam)
    scene.collection.objects.link(cam_obj)
    scene.camera = cam_obj
    return cam_obj


def place_camera(cam_obj, view):
    fx, fy, fz, visible = view
    tilt = math.radians(TILT_DEGREES)
    distance = visible / (2.0 * math.tan(math.radians(FOV_DEGREES) * 0.5))
    forward = (0.0, math.sin(tilt), -math.cos(tilt))
    cam_obj.location = (fx - forward[0] * distance, fy - forward[1] * distance, fz - forward[2] * distance)
    cam_obj.rotation_euler = (tilt, 0.0, 0.0)


def render_preset(preset, views, resolution=(1600, 900)):
    base = os.path.join(HERE, "out", preset)
    with open(os.path.join(base, "patch.json")) as f:
        data = json.load(f)
    reset()
    cam = setup_scene(resolution)
    mats = {}
    for name, part in data["parts"].items():
        spec = data["materials"].get(name)
        if spec is None:
            continue   # hidden walls
        if name not in mats:
            mats[name] = material(name, spec, base)
        add_part(name, part, mats[name])
    for view in views:
        place_camera(cam, VIEWS[view])
        bpy.context.scene.render.filepath = os.path.join(base, view + ".png")
        bpy.ops.render.render(write_still=True)
        print("rendered", preset, view)


def main(argv):
    views = list(VIEWS.keys())
    presets = []
    k = 0
    while k < len(argv):
        if argv[k] == "--views":
            views = argv[k + 1].split(",")
            k += 2
            continue
        presets.append(argv[k])
        k += 1
    for preset in presets or ["tr1"]:
        render_preset(preset, views)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    main(args)
