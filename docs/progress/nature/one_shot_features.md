# One-shot and special features

The special objects the lands hide: singing stones, weeping stones, idols, fireflies and the like, most tied to a
challenge or giving a reward once. The miracle bubbles and dispensers are covered in `../miracles/`.

**Progress: 1/8 done, 0 partial — 12%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place singing stones and their base | done | `MobileStaticArchetype` places them |
| Singing stones sing when lit or set right, with their own music (unconfirmed exact rule) | todo | the singing stone circle music is in the music list (`src/Audio/GameMusic.cpp`); nothing sets it off; see `../story/`; the quests: [the_singing_stones.md](../story/silver_scrolls/the_singing_stones.md), [the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) and the cut [the_miracle_stones.md](../story/silver_scrolls/the_miracle_stones.md) |
| The weeping stone and its reward | todo | placed as scenery only; see `../story/` |
| Idols and their rewards | todo | placed as scenery only; see `../story/`; Land 2's idol: [the_idol.md](../story/silver_scrolls/the_idol.md) |
| Fireflies come out at nightfall to hover by houses and street lights and hide in trees and rocks by day; lifting a tree or rock one hides in gives a one-shot miracle by the land's odds | todo | `CreateFireFly` and its reward odds are stubs; owned by [fireflies.md](fireflies.md) |
| The standalone altar | todo | placed as scenery only |
| The meteor | todo | |
| The creature cage | todo | see `../story/` |

Toys and how the creature plays with them are in [toys.md](toys.md).
