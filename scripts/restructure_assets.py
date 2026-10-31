"""Asset folder restructure (Oct 30, 2026): by kind instead of by scene.

  Characters/<Player|NPC|Enemy>/<Name>/      sheets + .hyanim side by side
  Models/MedievalVillage/                    the modular kit (unchanged inside)
  Models/<Architecture|Props|Terrain|Source>/ one-off models, each with its .textures/ folder and _*.hymat files
  Sprites/<Trees|Plants|Crops|Props|Effects|UI|_Sheets>/   things a SpriteRenderer draws
  Textures/<Ground|Backdrops|Palettes>/      surfaces for meshes and planes
  Materials/<Ground|Backdrops|Interior>/     scene materials (.hymat not tied to one model)
  Prefabs/<Characters|Effects>/   RenderProfiles/<Scene>/   Scenes/ and Fonts/ unchanged

Proposal and reasons: project doc EONLORE_Visual_Production_Guide_V9/ASSET_FOLDER_RESTRUCTURE_PROPOSAL.md.

Scenes, prefabs, animations and materials refer to assets by handle; the handle lives in the asset's .hymeta (or in
the file itself for .hyanim/.hymat/.hyprefab/.hyrender/.hyscene), and the AssetManager rebuilds handle -> path by
scanning. So every file moves together with its .hymeta and nothing that points at it changes. The few places that
name a path (two C++ scripts, the Python build tools, the kit scripts) are updated in the same run (TEXT_EDITS).

Usage (from the repo root, with Oxygen closed):
  python scripts/restructure_assets.py            dry run: prints every move, conflict, unmatched file and text edit
  python scripts/restructure_assets.py --apply    does it; writes _build/restructure/log.json (+ backups of edited files)
  python scripts/restructure_assets.py --undo     moves everything back and restores the edited files from the log
Then: git add -A (git sees the moves as renames), Oxygen --validate, open each scene, rebuild (two .h files changed).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path, PurePosixPath

REPO = Path(__file__).resolve().parents[1]
ASSETS = REPO / "Oxygen" / "project" / "Assets"
WORK = REPO / "_build" / "restructure"
SIDECAR = ".hymeta"

TREE = re.compile(r"^(Fir|Oak|Linden|Sakura|Pine|Birch|Maple|Willow|Cedar|Tree|tree|ancient_tree|cherry_blossom_tree)")
CROP_IN_FOLIAGE = re.compile(r"(Row\.png$|^Wheat_|^Pumpkins\.png$)")
ARCHITECTURE = ("ShrineHall", "ToriiGate")
TERRAIN = ("PathStone",)
FINALE_TILES = [("dirt_ground", "ShrineDirt.png"), ("meadow_ground", "ShrineMeadow.png"),
                ("mossy_shrine_stone_wall", "ShrineWallMoss.png"),
                ("stone_shrine_path", None)]                 # _0 -> ShrinePathA, _1 -> ShrinePathB


# ---------------------------------------------------------------- the map: old path (in Assets, posix) -> new path
def destination(rel, models_in):
    """New path for a file (sidecars are handled with their file), or None to leave it where it is."""
    p = PurePosixPath(rel)
    parts = p.parts
    name = p.name
    top = parts[0]

    if top in ("Scenes", "Fonts"):
        return None

    if top == "MedievalVillage" and len(parts) > 2 and parts[1] == "Models":
        return str(PurePosixPath("Models", "MedievalVillage", *parts[2:]))

    if top == "ShrineGlade":
        sub = parts[1] if len(parts) > 2 else None
        rest = parts[2:]
        if sub is None:
            if name == "Palette.png":
                return "Textures/Palettes/ShrineGlade_Palette.png"
            return None
        if sub == "Models":
            if rest[0] == "Source":
                return str(PurePosixPath("Models", "Source", *rest[1:]))
            model = longest_prefix(rest[0], models_in)
            if model is None:
                return None
            kind = "Architecture" if model.startswith(ARCHITECTURE) else "Terrain" if model.startswith(TERRAIN) else "Props"
            return str(PurePosixPath("Models", kind, *rest))
        if sub == "Foliage":
            if rest[0] == "Source":
                return str(PurePosixPath("Sprites", "Trees", "Source", *rest[1:]))
            return "Sprites/%s/%s" % ("Trees" if TREE.match(name) else "Plants", name)
        if sub == "Finale":
            if len(rest) == 1:                                   # torii_gate.png
                return "Sprites/Props/" + name
            group = rest[0]
            if group == "Tree":
                return "Sprites/Trees/" + name
            if group in ("Grass", "Flowers"):
                return "Sprites/%s/%s" % ("Trees" if TREE.match(name) else "Plants", name)
            if group == "Props":
                return "Sprites/Props/" + name
            if group == "Tiles":
                for key, new in FINALE_TILES:
                    if key in name:
                        if new is None:
                            new = "ShrinePathA.png" if name.endswith("_0.png") else "ShrinePathB.png"
                        return "Textures/Ground/" + new
            return None
        if sub == "Props":
            return str(PurePosixPath("Sprites", "Props", *rest))
        if sub == "Textures":
            if rest[0] == "Source":
                return str(PurePosixPath("Textures", "Ground", "Source", *rest[1:]))
            return ("Textures/Backdrops/" if name.startswith("SunsetBackdrop") else "Textures/Ground/") + name
        if sub == "Materials":
            return ("Materials/Backdrops/" if name.startswith("SunsetBackdrop") else "Materials/Ground/") + name
        if sub == "GladePack":
            return "Sprites/_Sheets/" + name
        if sub == "RenderProfiles":
            return "RenderProfiles/ShrineGlade/" + name
        if sub == "Characters":
            if rest[0] == "Source":
                who = "Player/Miko" if name.startswith("ShrineMaiden") else "NPC/Monk"
                return "Characters/%s/Source/%s" % (who, name)
            if name.startswith("Monk"):
                return "Characters/NPC/Monk/" + name
        return None

    if top == "FarmVillage" and len(parts) == 3:
        sub = parts[1]
        if sub == "Crops":
            return "Sprites/Crops/" + name
        if sub == "Foliage":
            return ("Sprites/Crops/" if CROP_IN_FOLIAGE.search(name) else "Sprites/Props/") + name
        if sub == "Props":
            return "Sprites/Props/" + name
        if sub == "Textures":
            return "Textures/Ground/" + name
        if sub == "Materials":
            return "Materials/Ground/" + name
        if sub == "RenderProfiles":
            return "RenderProfiles/FarmVillage/" + name
        return None

    if top == "Interiors" and len(parts) == 3:
        return {"Materials": "Materials/Interior/", "RenderProfiles": "RenderProfiles/Interiors/"}.get(parts[1], "?") + name \
            if parts[1] in ("Materials", "RenderProfiles") else None

    if top == "Characters":
        if len(parts) >= 3 and parts[1] == "Miko":
            return str(PurePosixPath("Characters", "Player", "Miko", *parts[2:]))
        if len(parts) == 3 and parts[1] == "Villagers":
            return "Characters/NPC/%s/%s" % (p.stem, name)
        if len(parts) == 2 and name.endswith(".hyanim"):
            return "Characters/NPC/%s/%s" % (p.stem, name)
        return None

    if top == "DioramaRPG" and len(parts) == 3:
        sub = parts[1]
        if sub == "Enemies":
            return "Characters/Enemy/%s/%s" % (p.stem, name)
        if sub == "Battle":
            return "Sprites/Effects/" + name
        if sub == "UI":
            return "Sprites/UI/" + name
        return None

    if top == "Battle" and len(parts) == 2:
        return "Sprites/Effects/" + name
    if top == "UI" and len(parts) == 2:
        return "Sprites/UI/" + name
    if top == "Effects" and len(parts) == 2 and name.endswith(".hyprefab"):
        return "Prefabs/Effects/" + name
    if top == "Prefabs" and len(parts) == 2 and name.endswith(".hyprefab"):
        return "Prefabs/Characters/" + name
    return None


def longest_prefix(entry, models):
    """The model an entry of a Models folder belongs to: '<Model>.glb', '<Model>.glb.hymeta', '<Model>.textures/...',
    '<Model>_<Material>.hymat'."""
    best = None
    for m in models:
        if entry == m + ".glb" or entry.startswith(m + ".") or entry.startswith(m + "_"):
            if best is None or len(m) > len(best):
                best = m
    return best


# ---------------------------------------------------------------- text edits (paths named in code and tools)
# (file relative to the repo AFTER the move, old text, new text). Every old text must be found (all occurrences change).
KIT = "Oxygen/project/Assets/Models/MedievalVillage/Source/MV_Kit/_scripts/"
TEXT_EDITS = [
    ("Oxygen/src/Scripts/ShrineScripts.h", '"ShrineGlade/RenderProfiles/', '"RenderProfiles/ShrineGlade/'),
    ("Oxygen/src/Scripts/ShrineScripts.h", '"FarmVillage/RenderProfiles/', '"RenderProfiles/FarmVillage/'),
    ("Oxygen/src/Scripts/ShrineScripts.h", '"Interiors/RenderProfiles/', '"RenderProfiles/Interiors/'),
    ("Oxygen/src/Scripts/VillageScripts.h", 'FindHandle("Effects/DustPuff.hyprefab")', 'FindHandle("Prefabs/Effects/DustPuff.hyprefab")'),
    ("Oxygen/src/Scripts/VillageScripts.h", "empty uses Effects/DustPuff", "empty uses Prefabs/Effects/DustPuff"),
    ("Hydro/src/Hydro/Scene/Vfx.h", '"Effects/DustPuff.hyprefab"', '"Prefabs/Effects/DustPuff.hyprefab"'),
    (KIT + "build_kit.py", r'ROOT = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\MedievalVillage")',
     r'ROOT = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\Models\MedievalVillage")'),
    (KIT + "build_kit.py", 'SRC = ROOT / "Models" / "Source" / "MV_Kit"', 'SRC = ROOT / "Source" / "MV_Kit"'),
    (KIT + "build_kit.py", r'SG = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets\ShrineGlade")',
     r'SG = Path(r"C:\Users\joeln\Hydro\Oxygen\project\Assets")   # Assets root since the Oct 30, 2026 restructure'),
    (KIT + "build_kit.py", 'SG / "Textures/Grass.png"', 'SG / "Textures/Ground/Grass.png"'),
    (KIT + "build_kit.py", 'SG / "Textures/Dirt.png"', 'SG / "Textures/Ground/Dirt.png"'),
    (KIT + "build_kit.py", '    p = SG / "Foliage" / fname\n',
     '    p = next((SG / "Sprites" / k / fname for k in ("Trees", "Plants") if (SG / "Sprites" / k / fname).exists()),\n'
     '             SG / "Sprites" / "Plants" / fname)\n'),
    (KIT + "build_kit.py", 'else ROOT / "Models"   # MV_OUT', 'else ROOT   # MV_OUT'),
    (KIT + "farm_kit.py", 'SG / "Textures/Grass.png"', 'SG / "Textures/Ground/Grass.png"'),
    (KIT + "probe.py", r'Assets\ShrineGlade")', r'Assets\Sprites\Plants")'),
    (KIT + "render_farm.py", r"Assets\MedievalVillage\Models\Source\MV_Kit", r"Assets\Models\MedievalVillage\Source\MV_Kit"),
    ("scripts/battle/fx_strikes.py", 'OUT_DIR = ASSETS / "Battle"', 'OUT_DIR = ASSETS / "Sprites" / "Effects"'),
    ("scripts/battle/fx_strikes.py", '"Battle/FX_Strikes.hyanim"', '"Sprites/Effects/FX_Strikes.hyanim"'),
    ("scripts/battle/build_arena.py", 'ASSETS / "Battle" / "FX_Strikes', 'ASSETS / "Sprites" / "Effects" / "FX_Strikes'),
    ("scripts/ui/build_game_ui.py", 'handle_of("UI/%s.png" % name)', 'handle_of("Sprites/UI/%s.png" % name)'),
    ("scripts/ui/build_game_ui.py", '"DioramaRPG/UI/BattleIcons.png"', '"Sprites/UI/BattleIcons.png"'),
    ("scripts/ui/make_ui_art.py", '"Assets", "UI")', '"Assets", "Sprites", "UI")'),
    ("scripts/quests/build_shrine_quest.py", '"DioramaRPG/Enemies/Slime.png"', '"Characters/Enemy/Slime/Slime.png"'),
    ("scripts/quests/build_shrine_quest.py", '"DioramaRPG/Enemies/Slime.hyanim"', '"Characters/Enemy/Slime/Slime.hyanim"'),
    ("scripts/interiors/build_inn.py", '"MedievalVillage/Models/', '"Models/MedievalVillage/'),
    ("scripts/interiors/build_inn.py", '"Interiors", "Materials"', '"Materials", "Interior"'),
    ("scripts/interiors/build_inn.py", '"Interiors", "RenderProfiles"', '"RenderProfiles", "Interiors"'),
]
OLD_KIT = "Oxygen/project/Assets/MedievalVillage/Models/Source/MV_Kit/_scripts/"
for _rel, _old, _new in TEXT_EDITS:   # an edit must not match its own result, or a second run would apply it twice
    assert _old not in _new, "text edit would apply twice: %s: %s" % (_rel, _old)


# ---------------------------------------------------------------- planning
def scan():
    files = []
    for root, dirs, names in os.walk(ASSETS):
        for n in names:
            files.append(Path(root, n).relative_to(ASSETS).as_posix())
    return sorted(files)


def plan(files):
    by_dir = {}
    for f in files:
        d = str(PurePosixPath(f).parent)
        if d.endswith("/Models") or d == "ShrineGlade/Models":
            by_dir.setdefault(d, set())
            if f.endswith(".glb") or f.endswith(".gltf"):
                by_dir[d].add(PurePosixPath(f).stem)
    moves, unmatched = [], []
    fileset = set(files)
    for f in files:
        base = f[:-len(SIDECAR)] if f.endswith(SIDECAR) else f
        parent = PurePosixPath(base).parts
        models = set()
        if len(parent) >= 3 and parent[0] == "ShrineGlade" and parent[1] == "Models":
            models = by_dir.get("ShrineGlade/Models", set())
        new = destination(base, models)
        if new is None:
            if in_new_layout(f) == False:
                unmatched.append(f)
            continue
        if f.endswith(SIDECAR):
            if base not in fileset:
                unmatched.append(f + "   (sidecar without its file)")
                continue
            new = new + SIDECAR
        if new != f:
            moves.append((f, new))
    return moves, unmatched


NEW_TOPS = ("Characters", "Models", "Sprites", "Textures", "Materials", "Prefabs", "RenderProfiles", "Scenes", "Fonts")


def in_new_layout(f):
    """True for a file that is already where the new layout wants it (so a second run reports nothing)."""
    parts = PurePosixPath(f).parts
    if parts[0] not in NEW_TOPS:
        return False
    if parts[0] == "Characters" and (len(parts) == 2 or parts[1] in ("Villagers", "Miko")):
        return False
    if parts[0] == "Prefabs" and len(parts) == 2:
        return False
    return True


def conflicts(moves, files):
    seen, problems = {}, []
    moving = {a.lower() for a, _ in moves}
    staying = {f.lower() for f in files} - moving
    for a, b in moves:
        k = b.lower()
        if k in seen:
            problems.append("two files want %s: %s and %s" % (b, seen[k], a))
        seen[k] = a
        if k in staying:
            problems.append("%s already exists (from %s)" % (b, a))
    return problems


def check_edits(applied):
    """Each edit: (file, found count). Looks at the post-move path, or where the file is now."""
    out = []
    for rel, old, new in TEXT_EDITS:
        path = REPO / rel
        if applied == False and path.exists() == False and rel.startswith(KIT):
            path = REPO / (OLD_KIT + rel[len(KIT):])
        if path.exists() == False:
            out.append((rel, old, -1))
            continue
        text = path.read_bytes().decode("utf-8").replace("\r\n", "\n")
        found = text.count(old.replace("\r\n", "\n"))
        if found == 0 and new.replace("\r\n", "\n") in text:
            found = -2                                           # already applied
        out.append((rel, old, found))
    return out


def oxygen_running():
    if os.name != "nt":
        return False
    try:
        out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Oxygen.exe"], capture_output=True, text=True).stdout
        return "Oxygen.exe" in out
    except OSError:
        return False


# ---------------------------------------------------------------- doing it
def replace_in_file(path, old, new):
    data = path.read_bytes().decode("utf-8")
    crlf = "\r\n" in data
    text = data.replace("\r\n", "\n") if crlf else data
    o, n = old.replace("\r\n", "\n"), new.replace("\r\n", "\n")
    count = text.count(o)
    text = text.replace(o, n)
    if crlf:
        text = text.replace("\n", "\r\n")
    path.write_bytes(text.encode("utf-8"))
    return count


def remove_empty_dirs(root):
    removed = []
    for d, subdirs, names in sorted(os.walk(root), key=lambda t: -len(t[0])):
        if Path(d) == root:
            continue
        if os.listdir(d) == []:
            os.rmdir(d)
            removed.append(Path(d).relative_to(root).as_posix())
    return removed


def apply(moves, edits):
    WORK.mkdir(parents=True, exist_ok=True)
    if (WORK / "log.json").exists():
        sys.exit("%s exists from an earlier run: undo it (--undo) or move it away first." % (WORK / "log.json"))
    backup = WORK / "backup"
    log = {"moves": [], "edits": []}
    for a, b in moves:
        src, dst = ASSETS / a, ASSETS / b
        dst.parent.mkdir(parents=True, exist_ok=True)
        os.replace(src, dst)
        log["moves"].append([a, b])
    for rel, old, new in edits:
        path = REPO / rel
        keep = backup / rel
        if keep.exists() == False:
            keep.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, keep)
            log["edits"].append(rel)
        replace_in_file(path, old, new)
    log["removed_dirs"] = remove_empty_dirs(ASSETS)
    (WORK / "log.json").write_text(json.dumps(log, indent=1), encoding="utf-8")
    return log


def undo():
    log = json.loads((WORK / "log.json").read_text(encoding="utf-8"))
    for rel in log["edits"]:
        shutil.copy2(WORK / "backup" / rel, REPO / rel)
    for a, b in reversed(log["moves"]):
        (ASSETS / a).parent.mkdir(parents=True, exist_ok=True)
        os.replace(ASSETS / b, ASSETS / a)
    remove_empty_dirs(ASSETS)
    print("Undone: %d moves, %d files restored." % (len(log["moves"]), len(log["edits"])))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true", help="move the files and edit the paths (default: dry run)")
    ap.add_argument("--undo", action="store_true", help="reverse an applied run from _build/restructure/log.json")
    ap.add_argument("--force", action="store_true", help="run even while Oxygen is open (not recommended)")
    ap.add_argument("--list", action="store_true", help="print every single move (default: a summary per folder)")
    args = ap.parse_args()

    if (args.apply or args.undo) and oxygen_running() and args.force == False:
        sys.exit("Oxygen is running: close it first (it rewrites .hymeta files and scenes), or pass --force.")
    if args.undo:
        undo()
        return

    files = scan()
    moves, unmatched = plan(files)
    problems = conflicts(moves, files)
    edits = check_edits(applied=False)
    missing = [(r, o) for r, o, c in edits if c == 0 or c == -1]
    done = [r for r, o, c in edits if c == -2]

    summary = {}
    for a, b in moves:
        key = (str(PurePosixPath(a).parent), str(PurePosixPath(b).parent))
        summary[key] = summary.get(key, 0) + 1
    if args.list:
        for a, b in moves:
            print("  %s  ->  %s" % (a, b))
    else:
        for (fa, fb), n in sorted(summary.items()):
            print("  %4d  %-48s -> %s" % (n, fa + "/", fb + "/"))
    print("\n%d files to move (with their .hymeta), %d left in place (Scenes, Fonts)." % (
        len(moves), len([f for f in files if PurePosixPath(f).parts[0] in ("Scenes", "Fonts")])))
    print("Text edits: %d, all found: %s%s" % (len(TEXT_EDITS), "yes" if missing == [] else "NO",
                                              " (%d already applied)" % len(done) if done else ""))
    for r, o in missing:
        print("  NOT FOUND in %s: %s" % (r, o.strip()[:90]))
    if unmatched:
        print("\nNot covered by any rule (left where they are):")
        for f in unmatched:
            print("  " + f)
    if problems:
        print("\nConflicts:")
        for p in problems:
            print("  " + p)

    if args.apply == False:
        print("\nDry run: nothing changed. Run again with --apply when the list looks right.")
        return
    if problems or missing or unmatched:
        sys.exit("\nNot applied: fix the conflicts / unmatched files / missing text edits above first.")
    if moves == [] and len(done) == len(TEXT_EDITS):
        print("\nNothing to do: the layout is already in place.")
        return
    log = apply(moves, [e for e, (_, _, c) in zip(TEXT_EDITS, edits) if c > 0])
    print("\nApplied: %d files moved, %d files edited, %d empty folders removed. Log: %s" % (
        len(log["moves"]), len(log["edits"]), len(log["removed_dirs"]), WORK / "log.json"))
    print("Next: git add -A, then Oxygen --validate (0 missing assets), open each scene, rebuild Oxygen.")


if __name__ == "__main__":
    main()
