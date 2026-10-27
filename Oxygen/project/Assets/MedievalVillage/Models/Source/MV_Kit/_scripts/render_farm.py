
import bpy
sc = bpy.context.scene
roofs = {}
for name in ("MV_House_A","MV_House_B","MV_Barn_A","MV_Shop_A","MV_Cottage_Thatch_A"):
    root = bpy.data.objects[name]; mats=set()
    for ch in root.children:
        if "Roof" in ch.name:
            for m in ch.data.materials:
                if m.name.startswith("MAT_Roof"): mats.add(m.name)
            if "LeanTo" in ch.name or "Gable" in ch.name: pass
    roofs[name] = sorted(mats)
print("ROOFS::", roofs)
sc.camera = bpy.data.objects["PV_FarmCam"]
sc.render.filepath = r"C:\Users\joeln\Hydro\Oxygen\project\Assets\MedievalVillage\Models\Source\MV_Kit\preview_farm.png"
sc.render.resolution_x, sc.render.resolution_y = 1600, 900
bpy.ops.render.render(write_still=True)
