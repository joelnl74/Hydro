# Orin Kenshi: art brief for his sprite sheets

Orin Kenshi, the stoic katana swordsman of the character bible (`Characters/Orin_Kenshi/CHARACTER_BIBLE.md` and its reference sheet in the EONLORE visual guide), joins the party in the countryside quest (Nov 1, 2026). Until his own art exists he is a **stand-in**: the Gate Guard's idle sheet recoloured navy, written by `scripts/characters/build_orin_standin.py`. This page says what to generate in PixelLab to replace it, and how the files go in without touching any scene or data file.

## What he looks like (from the bible)

Keep these, in this order, when the sprite has to simplify:

1. dark hair tied back;
2. a red scarf (the brightest colour on him; it should read at 96 px);
3. a single long sword (katana), sheathed at the left hip in the field, drawn in battle;
4. a navy, layered travel-armour silhouette (light armour, not a samurai's full kit);
5. bronze / gold trim on the shoulder and arm.

Around thirty, faint facial hair, composed and stern: upright posture, weight low, no flourishes. He is a precise single-target swordsman (counters, weak points), so his strikes are short and exact, not wide swings.

## Matching Miko

- **Same pipeline and look as Miko's v2 sheets**: PixelLab character, the same canvas and character size as Miko (she is drawn at 2.4 m in battle and 2.5 x 2.34 in the field; 96 px cells, feet near the bottom), the same outline weight and lighting from the upper left, the same muted, warm palette range (Miko's sheet `Characters/Player/Miko/Miko_Field.png` as the style reference).
- He should stand **a little taller** than Miko (about 1.1 x her height) and read darker overall (navy against her white and red), with the scarf as his accent.

## What to generate

| Export | PixelLab | Used for |
| --- | --- | --- |
| Character | Rotations, 8 directions (only the 4 cardinal ones are used) | The palette and the field idle |
| Idle | Breathing idle, 4 frames, 4 directions | Field: standing (`IdleDown` / `Right` / `Up` / `Left`) |
| Walk | Walking, 6 frames, 4 directions | Field (when he leads, later) and the battle's walk up and back (`WalkLeft`, `WalkRight`) |
| battle_idle | Spritesheet, west-facing: sword drawn, low guard, 6 to 8 frames | `BattleIdle` |
| slash | Spritesheet, west-facing: one fast draw-cut (iaido), 6 to 8 frames, the contact frame clear | `Slash` (Attack, Iaido, Wind Cutter, Falling Petal) |
| thrust | Spritesheet, west-facing: a straight thrust, 6 frames | `Thrust` (Crescent Thrust; set its `Clip` in `Abilities.hytable` once it exists) |
| death | Spritesheet, west-facing: drops to one knee, then down, 6 to 7 frames | `Death` |

West-facing like Miko's battle exports: the enemies stand on the left.

## Putting them in

The stand-in's handles are fixed in `build_orin_standin.py` (field texture 6290817403346251521, field animations 4415580237915726709, battle texture 7924613408950187331, battle animations 3182937116204411939). Writing the real sheets under the same handles replaces the stand-in everywhere at once:

- **Field**: `python scripts/characters/build_npc_idle.py <idle export> Oxygen/project/Assets/Characters/Player/Orin Orin_Field --walk <walk export> --handles 6290817403346251521 4415580237915726709` (rows Idle and Walk in four directions; `Npc Orin` plays `IdleRight`).
- **Battle**: `build_miko_battle.py` is written for Miko (her palette, the hair bell, her punch and kick picks); a hero version taking the exports above and writing `Orin.png` / `Orin.hyanim` in the same layout (96 x 96 cells, centred pivot, feet 15 px above the cell's bottom, clips `BattleIdle`, `WalkLeft`, `WalkRight`, `Slash`, `Thrust`, `Death`, and `Punch` / `Kick` as aliases of `Slash` for the plain Attack's alternation) is a small change once the exports exist.
- **Portrait**: `Orin.hydef`'s `Portrait` is the field sheet; `PortraitSprite` 0 is `IdleDown_0`.
- Then delete `build_orin_standin.py` (or keep it only as a fallback) and note the change in `docs/TestBed.md`.
