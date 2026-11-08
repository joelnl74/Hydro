# Texel density of every JP_Kit prefab and prop, per material (32 texels per metre +-20 %); run headless on JP_Kit.blend.
import bpy, math, json
from collections import defaultdict
res, bad = {}, []
for col in ("90_Prefabs", "09_Props_3D"):
    for ob in bpy.data.collections[col].objects:
        if ob.type != 'MESH' or ob.name.startswith("COL_"):
            continue
        me = ob.data
        uv = me.uv_layers.active.data
        acc = defaultdict(lambda: [0.0, 0.0])
        for p in me.polygons:
            m = me.materials[p.material_index]
            img = next((n.image for n in m.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image and "_EMI" not in n.image.name), None)
            if img is None or img.size[0] <= 4 or p.area < 1e-4:
                continue
            us = [uv[li].uv for li in p.loop_indices]
            a = sum(abs((us[i].x - us[0].x) * (us[i + 1].y - us[0].y) - (us[i + 1].x - us[0].x) * (us[i].y - us[0].y)) / 2 for i in range(1, len(us) - 1))
            acc[m.name][0] += p.area
            acc[m.name][1] += a * img.size[0] * img.size[1]
        res[ob.name] = {}
        for mname, (wa, ta) in acc.items():
            d = math.sqrt(ta / wa)
            res[ob.name][mname] = round(d, 1)
            if d < 25.6 or d > 38.4:
                bad.append([ob.name, mname, round(d, 1), round(wa, 2)])
allv = [d for o in res.values() for d in o.values()]
print("RESULT::" + json.dumps({"objects": len(res), "min": min(allv), "max": max(allv), "n_outside": len(bad), "outside": bad}))
