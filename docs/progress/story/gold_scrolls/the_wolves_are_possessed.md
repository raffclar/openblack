# The Wolves Are Possessed

A gold story event on the third land: after the player has won two villages and stopped two of the creature's prison
pillars, Lethys sends a pack of twenty possessed wolves at the Indian village. If the player has finished the monk's
quest, the monk appears and turns most of the wolves into cows. The player must kill the rest before any reaches the
village, or the village's faith swings to Lethys. It is logged in the story log under the monk's line "The wolves are
possessed." (it has no title of its own) and has no scroll to click.

**Land:** 3 · **Giver:** Lethys (a cut scene; no scroll) · **Script:** FreeTheCreature (its wolf attack) · **Reward:** none; failing gives the Indian village to Lethys · **Repeatable:** no

Part of freeing the creature: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md).
The monk is the silver scroll [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md). The land:
[../land_3.md](../land_3.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`) and the
game's text table. openblack never runs Land 3's control script (the land-loading command does nothing), and flocks,
animals, villagers, dialogue, snapshots and belief changes are stubs in `src/CHLApi.cpp`, so every row is todo unless
the notes say otherwise.

**Progress: 0/28 done, 0 partial — 0%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started by the creature's prison when exactly two pillars are down and the Indian village is the player's; if the Indian village is won third, it never happens | todo | the prison script's main loop |
| At the same moment, if the monk's quest is done, his Wonder gift is started | todo | see [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md) |
| It logs to its own story-log entry, separate from freeing the creature, and switches back to the creature's entry when it ends | todo | `Snapshot` is a stub |
| It waits 5 minutes, then waits until the Indian village is the player's (or gone) | todo | |
| If all three pillars are down by then, nothing happens and the entry is simply marked complete | todo | |

## The attack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pack is formed at a spot about 510 from the Indian village, towards the prison: wolves are made within 20 of it until there are 20, held loosely together (inner radius 5, outer 40) | todo | `FlockCreate`, `FlockAttach` are stubs; creating animals is not supported by `Create` |
| The pack sets off slowly (half speed) towards the Indian village | todo | |
| A cut scene with one of the generic quest tunes: the camera watches from behind and follows the pack. Lethys: "You may have your few believers." Lethys: "But I can still command the beasts of the wilderness." | todo | `RunText` is a stub |
| The camera moves; the pack quickens a little (0.6) | todo | |

## The monk helps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if the monk's own quest has reached its end: the Tibetan town music plays, and after 4 seconds the monk appears near the pack's path facing the camera, playing his ambient animation five times | todo | creating villagers is not supported |
| Monk: "It seems you have need of me." Monk: "The wolves are possessed." | todo | |
| The camera follows the pack again; wolves are taken from it until 8 are left. Each taken wolf, after 1 to 4 seconds, stops, a sparkle appears and it is replaced by a cow, which is let go | todo | |
| Monk: "But I think I can help you. I will try to destroy some of the wolves." Monk: "That is all." The monk disappears | todo | |
| Without the monk, the scene just lasts 12 seconds more and all 20 wolves come | todo | |
| The story-log entry is started at nothing done, its reminder the good advisor's "The Village is coming under attack!" | todo | |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Kill every wolf of the pack before any of them gets within 25 of the spot at the edge of the village; the check is every 5 seconds | todo | the creature is still held in the prison, so the player has only miracles and the hand |
| There is no time limit other than the wolves' walk | todo | |

## Failure: the wolves reach the village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack is let loose; a cut scene flies the camera to the village | todo | |
| The story-log entry is marked complete (the source's own comment calls it "you have failed to save the town") | todo | |
| The good advisor steps out and points: "The wolves have reached the Village!" then "The Villagers are switching allegiance!" | todo | |
| Lethys's belief in the Indian village is set to full and the player's to 0.3 | todo | `ObjectRelativeBelief`, `SetPlayerBelief` are stubs |
| This can lose the village, raising its prison pillar again | todo | see [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the last wolf of the pack is dead the entry is marked complete; there is no line and no reward | todo | |
| Either way the entry ends at complete with no alignment change, so the log cannot tell success from failure | todo | |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It cannot block the story: freeing the creature does not wait for it | todo | |
| Losing the village only means winning it back | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The second line after the wolves arrive belongs to the evil advisor in the text table, but only the good advisor has stepped out to say it | todo | |
| The wait for the village to be taken over has no pause in its loop | todo | |
