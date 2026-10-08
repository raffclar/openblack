# Alignment

How good or evil each player is, from -1 (evil) to 1 (good). Everything the player does to the world moves it, and the
world shows it back: the sky, the hand, the temple, the land and the music.

**Progress: 8/18 done, 1 partial — 47%**

## The value

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player has an alignment from evil to good, starting neutral | done | `src/ECS/Components/Alignment.h`, `AlignmentSystem`, test `test_alignment` |
| A change comes through over the turns, at most so much a turn, and the rest is lost | done | `AlignmentSystem::UpdateTurn`, test `test_alignment` |
| The further a player leans, the less a change the same way moves them and the more one the other way does | done | `DampAlignmentChange` (`src/Magic/AreaEffect.cpp`) |
| Alignment falls into bands (very good to very evil) that the visuals and music follow (unconfirmed bands) | todo | |
| Scripts set and read a player's alignment, with a cap on each change | todo | stubs in `src/CHLApi.cpp` |

## Deeds that move it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle moves its caster by what it reached: animals (kind or nasty), villagers, creatures, buildings, plants, fields, features and the land each by their own amount | done | `MagicSystem.cpp`, `AlignmentWeight` |
| Impressing villagers moves the player by the kind of thing they saw | done | `ReactionSystem::Impress` |
| Planting a tree with the water miracle is good | done | `GameMagicWorld::PlantNear` |
| Pulling up a tree or planting one by hand | todo | the hand can't hold trees yet |
| Each villager death moves its killer by how it died (thrown, burnt, drowned, eaten …) | todo | the death reason table is loaded, unused |
| Throwing or squashing villagers and animals | todo | needs physics, see `../physics/` |
| Sacrificing at the worship site | todo | see [prayer power](prayer_power.md) |
| Feeding and giving wood to villagers by hand | todo | see `../resources/resource_handling.md` |
| What disciples do moves their god (unconfirmed which jobs) | todo | |
| What a creature does moves its own alignment, not its god's | done | `MagicSystem.cpp`; see `../creature/` |

## What it shows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky turns dark or bright with the alignment of whoever holds the land at the camera | partial | the sky drifts (`AlignmentSystem::Update`), from player one's alignment only; see `../sky/` |
| Crops and trees grow faster on good land | done | `FieldSystem`, `VegetationSystem` |
| Doves circle a good temple, bats an evil one | todo | see `../animal/birds.md` |

How the hand, the temple and the music change with alignment is in `../hand/`, `../temple/` and `../audio/`.
