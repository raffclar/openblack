# Creature cave room

The temple's room for the creature. Everything about the cave itself (the creature, its scrolls, belts and medals,
tattoos) is in ../creature/creature_cave.md; this file only lists the room's part of the temple.

**Progress: 3/6 done, 1 partial — 58%**

## The room in the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A door of the main room leads to the cave | done | `TempleInterior::EnterRoom` |
| The waterfall slides and sounds, the fire crackles where the creature stands | done | `TempleInterior.cpp` |
| Flames, smoke and the waterfall's spray and mist | done | `src/3D/CreatureCaveEffects.cpp`; test `test_creature_cave_effects` |
| The cave's contents | partial | see ../creature/creature_cave.md |
| Clicking the creature opens the tattoo editor | todo | TODO in `TempleCameraModel.cpp`; see ../creature/creature_tattoos.md |
| The dummies start the creature's fight practice when no temple script runs | todo | TODO in `TempleCameraModel.cpp` |
