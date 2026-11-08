# Scene review, 2026-10-25

**Screenshots:** the battle (1919 x 1079) and the statue village (801 x 664) from the build of 2026-10-25; they are not stored in the repository. This review comments on the *visible screenshots*. It does not assert that upcoming engine features are already implemented.

> Progress since this review: battle item 2 (96 x 128 hero sprites), item 3 (contact shadows, feet on one ground row) and the "one-evening test" (Orin heavy slash, Lila heal) are done; see the [plan](DioramaRPG.md). The rest is open.

## Verdict

The visual identity has good ingredients but the screenshots currently look like **traditional, polished 2D pixel-art RPG scenes**, not yet the mixed-depth, carefully staged diorama look of *Octopath Traveler II* and *Final Fantasy Resonance*. That is expected because 3D is under development. **Do not throw away the current scenes**; the next step is a controlled 2D art/readability pass and richer authored sprite poses.

## Statue village - observations

**What works**
- Statues/benches/planters are coherently placed around a recognizable plaza landmark.
- Cobblestone variations and moss create a lived-in town ground surface.
- Top-down layout is clear for immediate playtesting.

**What needs work, priority order**
1. **Ground noise**: almost every cobble competes for attention. Add calmer value regions around character feet and quest markers. At screen scale aim for large texture clusters rather than every stone equally highlighted.
2. **Characters disappear**: field figures are small relative to elaborate ground. Test thicker but selective silhouette separation; dark shoe anchors + mild brighter/clothing focal colors. Don't just add saturated neon outlines.
3. **Layout reads like a flat room**: add near/mid/far occluding architectural elements *now*; in 3D, use upright building masses and non-orthogonal camera/parallax.
4. **Focal point**: statue is strong, but its surrounding area needs more directed light and cleaner ground contrast to lead player attention naturally.
5. **Geometry/storytelling**: add asymmetric human set-dressing: a cart or noticeboard on one side, a brighter lantern near a specific NPC, a small patch of missing cobbles. Avoid identical props evenly spaced.
6. **Shadows**: add foot contact shadows for both NPCs and planters/statue to anchor them. The dark central area should feel like cast shade, not a hard radial game overlay.

**Two-hour 2D test**: reduce 25-35% of bright cobble highlights around the two heroes; add clear contact shadows; give one interaction NPC an accent accessory and a small highlight region. Screenshot before/after at same zoom.

## Battle - observations

**What works**
- Turn order strip immediately communicates the structure of combat.
- Target pointer, shield pips and HP/MP status are present.
- Magic Wisp silhouette is readable and on-brand for a forest shrine fight.
- Background silhouettes and dark canopy create an attractive mood.

**What needs work, priority order**
1. **Combat stage**: the brown oval reads like a flat token arena. Replace with an irregular dirt/grass/cobble stage where combatants share the same ground perspective; include rooted rocks, shrine debris and foreground grass.
2. **Hero sprites too simple**: Orin is mostly rectangular, with stiff sword pose; Lila's staff/spear needs its distinctive weapon and cast posture. Move toward separate **96 x 128** battle sprite atlases and clear acting poses.
3. **Character plane vs floor**: align feet on believable ground contact points, add localized shadows and make depth placement intentional. Orin and Lila should not seem pasted onto the arena.
4. **UI emphasis**: the top turn strip has lots of dead empty width. Give the active actor an obvious, attractive state; reduce filler width. Command panel at bottom should display cursor/selected action more clearly.
5. **Signal hierarchy**: weakness icons are tiny and shields compete with HP bars. On attack, temporarily emphasize the enemy's WEAK / RESIST feedback near impact while keeping status permanently accessible.
6. **Backdrop and lighting**: forest silhouette layers are atmospheric but quite flat. Add distinct midground silhouettes, a point-lit rock/shrine, and foreground vignette. Real 3D depth can come later.
7. **Effects timing**: even tiny hit-stop (30-70 ms), wind-up, attack travel, impact sparks, brief screen shake on heavy hits can sell action. Avoid eye-catching VFX before stronger key poses.

**One-evening 2D test**: make a single **Orin heavy slash** (idle / windup / 2 strike frames / impact / recover) and a **Lila white-magic heal**. Then replace the oval with a more grounded painted encounter stage. Evaluate again at runtime resolution.

## Compare against the actual target (not a clone)

| Quality | Prototype currently | Target impression | To do now / later |
|---|---|---|---|
| Pixel clarity | Strong but uneven detail density | authored clusters and expressive silhouettes | **Now** |
| Environment lighting | dark mood, some warm accents | spatially motivated warm/cool light | **Now** light map; **Later** 3D shadows |
| Depth | layer stacking and darkness | believable layered space, occlusions and perspective | **Now** parallax/occlusion; **Later** 3D |
| Hero acting | few rigid poses | legible motion silhouettes and combat personalities | **Now** |
| Battle staging | flattened oval field | grounded, cinematic sense of place | **Now** art; **Later** 3D battle stage |
| Post-process | dark framing approximates depth | deliberate bokeh/DOF, bloom and atmosphere | **Later**, selective |
| Camera | overhead fixed | region-appropriate angle & cinematic cuts | **Later** |

## Screenshot signoff checklist

- [ ] At zoomed-out viewport, the hero is the first readable shape after landmark.
- [ ] All character feet are properly grounded and shadowed.
- [ ] 2D pixel density and sprite edge scale match across scene elements.
- [ ] Landmarks/interactive objects readable without glowing arrows everywhere.
- [ ] Battle feels placed in the world rather than overlaid on a generic floor stamp.
- [ ] Enemy weakness, shield count, selected command can be understood in 2 seconds.
- [ ] Real-time/painted lighting does not bleach bright clothing or swallow dark outfits.
- [ ] Scene remains readable in grayscale (value separation test) and on smaller monitors.

## External reference notes

- Octopath Traveler II: official overview, 2D pixel art fused with 3DCG - https://www.square-enix-games.com/en_US/games/octopath-traveler-ii
- Final Fantasy Resonance: Square Enix's official Japanese description describes its hybrid style of 2D sprites plus pixelized 3D cinematic scenes - https://www.jp.square-enix.com/topics/detail/4052/
- FF Resonance creative interview: horizontal camera angles, tall pixel character silhouettes and bolder cinematic cuts - https://www.crunchyroll.com/ko/news/interviews/2026/10/20/final-fantasy-resonance-interview

Do not distribute their game screenshots or rip their assets into this repository. Use references to clarify quality goals and production techniques.
