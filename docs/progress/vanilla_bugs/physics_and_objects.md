# Vanilla bugs: physics and objects

Bugs in the original game's physics engine and its handling of objects: bodies, collisions, breaking buildings, the
hand's grip and reward chests. Most come from the physics research (read from the executable), which is ported on the
physics branch; picking and land queries are in [engine.md](engine.md).

**Bugs: 7 (soft-locks and lost progress: 0)**

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| A body's moment of inertia has one cross term computed from the wrong pair of axes (x and z where it should be y and z) | Lopsided objects tumble slightly differently from the physics they were meant to have | The tensor set-up writes the x-z product into the y-z entry | [object_dynamics.md](../physics/object_dynamics.md) | reproduced |
| A reward chest's landing shake is centred on the world origin instead of the chest | The thump only shakes a camera within 200 m of the map's corner, so the player normally feels no shake | The reward's landing creates its camera shake at (0, 0, 0); read from the executable (the progress rows still say a small shake happens) | [rewards.md](../story/rewards.md), [explosions.md](../physics/explosions.md) | undecided |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The "thrown at a building" reaction only runs when the building's resting physics entry is flagged as coming from the hand, which a building's entry never is | nothing visible: the branch that would let the creature learn from the throw is dead in practice | The resting entries are always made with a plain flag; read from the executable | [damage_and_repair.md](../building/damage_and_repair.md) | undecided |
| When a flying building piece merges back into its parent, the function returns an uninitialised value as "the object that stays" | nothing visible found | Read from the executable | [damage_and_repair.md](../building/damage_and_repair.md) | undecided |
| When a building breaks into more than 64 groups, a triangle left unlabelled reads the anchor table one place before its start | Undefined; in practice the triangle is removed | Read from the executable | [damage_and_repair.md](../building/damage_and_repair.md) | undecided |
| A tree body's ring set-up has cases for an inner and an outer ring that its loop never reaches | nothing visible | The loop runs over the middle three rings only | [object_dynamics.md](../physics/object_dynamics.md) | undecided |
| The hand's strength is meant to scale with a hand value that is only ever written as 0 | nothing visible: the hand always pulls with the same force | The only writer sets it to 0 at start-up; read from the executable | [hand_physics.md](../hand/hand_physics.md) | reproduced |
