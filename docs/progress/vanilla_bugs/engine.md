# Vanilla bugs: engine

Bugs in the original game's general engine code: picking under the cursor, land queries, animation clips and the intro
films. Most come from the physics research, read from the executable; where no progress file mentions the bug yet, the
Found in column names the nearest one.

**Bugs: 5 (soft-locks and lost progress: 0)**

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Picking a drawn object under the cursor takes the first triangle that contains the cursor, not the nearest one within that object | The cursor can pick a point on the far side of an object, so the hand or tooltip lands slightly wrong on overlapping parts | Read from the executable's draw-time picking (physics research; not yet in a progress file) | nearest: [placement.md](../hand/placement.md) | undecided |
| The land point under the cursor always splits each land cell along the same diagonal, whatever the cell's own split | On cells drawn with the other diagonal, the picked point on the land can sit slightly off the drawn ground | The cursor's land test uses a fixed diagonal while drawing and the land height use the cell's split; read from the executable (physics research; not yet in a progress file) | nearest: [look_and_morph.md](../hand/look_and_morph.md) | undecided (the research notes call it a quirk to keep) |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The land ray walk's "no extension needed" test compares a single-precision value with a double-precision 1e20, which never matches | nothing visible found: the ray is always extended to the map edge (a vertical ray's height is pushed out by 1e20) | Read from the executable (physics research; not yet in a progress file) | nearest: [look_and_morph.md](../hand/look_and_morph.md) | undecided |
| A clip played once that is advanced past its end replays its events from the start | nothing visible: callers stop advancing a finished one-shot clip | Read from the executable's clip event loop (physics research; not yet in a progress file) | nearest: [animation.md](../creature/animation.md) | undecided |
| The intro film fades out from 58 s and ends at 60 s | Its last 160 frames are never seen | Read from the intro's playback | [bink_playback.md](../video/bink_playback.md) | undecided |
