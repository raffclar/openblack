# Modified data packs

The `AllMeshes.g3d` of the installation is a mod by the user, not the original pack: here are its differences from the
original (embedded textures, rock origins, villager textures) and what was found out about the villager meshes.

- [The installation's pack](#the-installations-pack)
- [Embedded textures](#embedded-textures)
- [Mesh origin (floating rocks)](#mesh-origin-floating-rocks)
- [Villagers: meshes and textures](#villagers-meshes-and-textures)
- [Future ideas (need the SDK)](#future-ideas-need-the-sdk)
- [Pending](#pending)

## The installation's pack

- The current `Data\AllMeshes.g3d` is a mod by the user (626 meshes, textures 0x01–0x70). There is no copy of the base
  pack.
- References for original meshes: the Creature Isle pack (`...\CreatureIsle\Data\AllMeshes.g3d`, **different
  indices**: check its `AllMeshes.h`).
- The `Ultimate\Data\AllMeshes.g3d` pack is another mod (704 meshes).

## Embedded textures

- The palm trees (meshes 586–589) carry their texture embedded with id **0x1001**, but their material asks for 0x85–0x88, which do not
  exist in the pack. Ultimate repeats the pattern. In the original game they look fine; openblack uses the mesh's embedded
  skin when the material does not find its texture.
- In `LH3DMesh::Create` (0x806460) the embedded skins are registered with `fn_008379E0`, and the value 0x1001 could be
  format + id (**inferred**).

## Mesh origin (floating rocks)

- The mod's rock meshes have their origin shifted relative to the original ones:
  `MSH_Z_SPELLROCK01` lowest vertex +0.8 (original −0.6), `MSH_BOULDER3_LIME` +0.2 (original −0.2).
- The script altitudes are designed for the original meshes, so with the mod they end up in the air (in
  Land1, 74 of 243 static objects float more than 5 cm). The original does not correct it.
- In addition, many of the mod's rocks end up resting on a tip because of their rotation: even though the lowest vertex touches the ground,
  they seem to float.

## Villagers: meshes and textures

**Meshes**

- `MSH_P_*` meshes 413–524 of the base pack (geometry identical to Creature Isle's, so the user's mod does not touch
  them). Each tribe and sex has **its own mesh**, in 3 levels: `_1` ~260–300 vertices, `_2` ~110–150, `_3`
  26–40. There is one texture per tribe, shared by man and woman (0x4E–0x5C). Some meshes repeat geometry with
  another texture (TIBETAN=TIBT, JAPANESE=JAPN, SHAOLIN_MONK=JAPN_M_A_1, TAN/WHITE girls, INTRO_M=CULT_PRIEST).
- `MSH_P_INTRO_M`/`_F` (483/484) **do not have more polygons**: 258 v / 348 t, like a normal villager. INTRO_M is the
  CULT_PRIEST geometry with texture 0x59; INTRO_F uses the female villagers' skeleton. Their textures are 256².
  `INTRO.bik` is a pre-rendered video: its models are not in the files.
- Skeleton: 110 of the 112 meshes have the same 22-bone hierarchy (EGPT_M_B_2 has 21), and all the
  `M_P_*` animations of AllAnims.anm (232) are 22-bone. The rest poses vary a little (groups: 85 meshes,
  12 female, 10, CULT_PRIEST+INTRO_M).
- Which mesh is drawn: the original, always LOD 1 (the LevelOfDetail loads are disabled): `stdDetail` /
  `childMeshMedium` for villagers (about 5.7 KB of mesh versus 12 KB for the high one) and `std` for animals. openblack
  does the same since 2026-09-30 (**faithful**). History: previously openblack used only `highDetail` (no LOD) and drew the
  villagers in the rest pose (Renderer.cpp, "Get animation frame instead of default"; L3DAnim loaded AllAnims but
  there was no playback, today see [animation.md](animation.md)).

**Textures**

- All are **native** 256² atlases. The ones the user's pack has at 512 or 1024 are upscales with
  duplicated pixels (the error versus doubling their half is < 1.5 levels), except 0x5A and 0x47 (native 512).
- INTRO_M/F look finer because their 256² texture is of **a single** character (the villagers: 4 per atlas).
  No pack (base, Creature Isle, Ultimate) comes with better ones.
- **The Norse atlas of the user's pack (0x5A, 1024 px) is not the original**: it has the intro characters
  painted on it (the face of the blonde woman of INTRO_F, the bearded man, one with a red shirt and jeans).
  The Creature Isle Norse atlas (skin 0x74 of its pack, `MSH_P_NORS_F_A_1` = 580 there) has the original clothing
  (dark dress, men in black with a belt). That is why the female villagers of Land1 (Norse village) look like "the ones from the
  intro": the mesh (`NORS_F_A_1`, 498) and the clip (`M_P_Walk_Woman`) are the correct ones.

## Future ideas (need the SDK)

These ideas from the user's task list wait for a real mod SDK, built once the game is reconstructed. They are **not**
pending engine work: the engine stays faithful to the original, and each idea would be disabled by default.

- **Better physics** (see [physics.md](physics.md#pending)): building pieces with a collision mesh, so that they hit
  other objects; building pieces that can be grabbed with the hand; more improvements as they come up.
- **Animals always flee** (see [animals.md](animals.md)): the predators repeat their flee reaction every turn (what
  `Reaction::ProcessReactions` would do, switched off in the original by a debug switch), so the herbivores flee every
  time a predator comes near and not only when it is created. Today only the test hook
  `OPENBLACK_TEST_SPREAD_REACTIONS=<turn>` exists (see [Test hooks](animals.md#test-hooks)).

## Pending

- Floating rocks resting on a tip: planned solution: physics (**not checked** whether it has already been done).
