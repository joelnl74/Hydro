# Bright, neutral inspection renders of the kit prefabs (not the style look): run headless on MV_Kit.blend.
import bpy, math
from mathutils import Vector
from pathlib import Path
SRC = Path(bpy.data.filepath).parent
scene = bpy.context.scene
scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.35, 0.38, 0.42, 1)
scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
sun = bpy.data.objects["PV_Sun"]; sun.data.energy = 3.5; sun.data.color = (1, 0.95, 0.88)
scene.view_settings.exposure = 0.0
scene.render.resolution_x, scene.render.resolution_y = 1600, 900
shots = []
for name, target, d, dist, lens in (
        ("inspect_front", (10.0, 7.0, 2.5), (0.0, 0.85, -0.5), 40, 35),
        ("inspect_back", (10.0, 9.0, 2.5), (0.45, -0.75, -0.48), 40, 35),
        ("inspect_props", (0.0, -54.0, 1.2), (0.6, 0.45, -0.6), 26, 35)):
    cd = bpy.data.cameras.new(name); cd.lens = lens
    cam = bpy.data.objects.new(name, cd); scene.collection.objects.link(cam)
    dv = Vector(d).normalized()
    cam.location = Vector(target) - dv * dist
    cam.rotation_euler = dv.to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    scene.render.filepath = str(SRC / (name + ".png"))
    bpy.ops.render.render(write_still=True)
    shots.append(name)
print("RESULT::" + repr(shots))
