# Vanilla bugs: miracles

Bugs in the original game's miracles, their effects and the miracle dispensers given as rewards. The fireflies and
vortex research found no miracle bugs: the vortices are in [story_scripts.md](story_scripts.md) and the skirmish map
firefly data in [multiplayer_and_skirmish.md](multiplayer_and_skirmish.md).

**Bugs: 2 (soft-locks and lost progress: 0)**

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The shared dispenser reward script's "if a refill period was given" check tests the game time instead of the period | Every reward dispenser has its refill time set to whatever the quest passed, even 0 (the Hermit passes 0; the Explorers' note says the engine ignores 0 for such a dispenser; undetermined for the Hermit's) | `Reward.txt`: the check reads `time > 0` rather than the period argument (checked in the script source) | [dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md), [the_hermit.md](../story/silver_scrolls/the_hermit.md), [the_explorers.md](../story/silver_scrolls/the_explorers.md), [the_sea.md](../story/silver_scrolls/the_sea.md) | undecided |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Water rings come from a fixed pool of slots, and a reused slot keeps drifting with the wind | Old blast and water-miracle rings on the sea drift away with the wind | Per the miracle research, a reused ring slot drifts with the wind | [blast.md](../miracles/blast.md), [water.md](../miracles/water.md) | differs: not reproduced (openblack has no fixed ring pool) |
