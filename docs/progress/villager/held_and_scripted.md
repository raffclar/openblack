# Villagers in the hand and under script

The player can pick villagers up, carry, drop and throw them, and the challenge scripts can take villagers over to walk
them, play animations and gather crowds. These rows cover the villager's side; the hand and physics own the rest.

**Progress: 2/16 done, 2 partial — 19%**

## In the hand and thrown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager can be picked up by the hand, and stops what it was doing | done | `HandGrabSystem::Take` (IN_HAND, previous state kept, `VillagerMemory`) |
| A held villager struggles, and the villagers around react | todo | |
| A villager put down carefully stands and goes back to deciding (or becomes a disciple) | todo | See disciples.md |
| A thrown villager flies, lands, and gets up or dies from the fall | partial | Flies (FLYING), lands with its posture, decides after its landing clip or dies (`villager_physics`); its clips aren't drawn |
| A villager dropped in the sea drowns | done | `villager_physics::Sink`, `Drowning` |
| Picking up and throwing villagers moves the player's alignment | todo | See ../worship/ |
| A villager can be picked up by the creature and controlled by it | todo | See ../creature/ |
| A villager carried off by a tornado | partial | See ../miracles/ |

## Under script

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script moves a villager to a place and waits until it arrives | todo | |
| A script plays an animation on a villager and waits for it to finish | todo | |
| A script makes a villager wander round a point | todo | |
| A script makes a villager walk along a path | todo | |
| A script puts villagers in a crowd | todo | |
| A script releases a villager, which goes back to its own life | todo | |
| Script-made dances (the dance editor's dances) | todo | |
| Script asks whether a villager is drowning, at home, a child and the like | todo | See ../story/ |
