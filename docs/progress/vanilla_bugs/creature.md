# Vanilla bugs: creature

Bugs in the original game's creature code and creature data: its reactions, actions and development. The guide's
lesson scripts are in [story_scripts.md](story_scripts.md); creature swaps in [silver_quests.md](silver_quests.md).

**Bugs: 3 (soft-locks and lost progress: 0)**

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Outside a fight, a creature struck by a thrown object or an effect has its fear and anger raised twice: once by its physics impact reaction and once by its cut-and-scarred reaction, which spares only an effect cast by its own player | A struck creature becomes twice as frightened and angry as one source would make it | Both reactions run for the same blow; read from the executable in the physics research (not yet a row of its own in a progress file) | [impact_damage.md](../physics/impact_damage.md) | reproduced on the physics branch (per the physics research spec) |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The creature's smash treats a living target as a rock, which fails, so the branch meant to play a body-hit sound never runs | Smashing a villager or animal makes no smash sound and does nothing | The smash's living branch picks one of three samples but casts the target to a rock first; read from the executable | [rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) | reproduced |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The "catch a fireball and throw it back" creature action is listed but always fails | nothing visible: creatures never do it | Read from the executable | [feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md), [object_actions.md](../creature/object_actions.md) | reproduced |
