# The Beach Temple Puzzle

The second land's beach silver scroll: a man in a small temple of four stacked rings on the beach asks the player to
rebuild it, ring by ring, on the column furthest up the beach so the floods can't reach it. Once the temple stands on
the far column, the man makes it heal every living thing that comes near it, for the rest of the land. The ring rules
themselves are in [../minigames.md](../minigames.md#beach-temple-rings-land-2); this file covers the quest around them.

**Land:** 2 · **Giver:** a Greek farmer living in the small temple on the beach · **Script:** HanoiFlood · **Reward:** the temple heals every living thing within 10 of it, for good · **Repeatable:** no

Sources: the quest's script source (`HanoiFlood.txt`) and the shared helpers it runs (the notify loop with an end
variable, the signpost helper and the standard reminder), each checked line by line against the PC game's compiled
`challenge.chl` (argument order, text numbers, the challenge number on the scroll and snapshots); the land's control
script for the start; the game's English text table for every line; the puzzle object's class list in the PC executable
for whether any flood exists. openblack is judged on the physics work tree (`ob-wt-physics`): of the 42 script
functions the quest's scripts use, 30 still only log "not implemented" in `src/CHLApi.cpp`, among them the scroll, the
puzzle's "played" test, dialogue, the camera moves, the snapshots and the influence ring; making a puzzle object also
does nothing (`CreateScriptObject` makes only scenery and rocks). The land's control script also stops before this
quest's start (see [../land_2.md](../land_2.md) row 1), so nothing below happens; every row is todo unless the notes
say otherwise.

**Progress: 0/49 done, 2 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is started at once when the land begins, right after the land's set-up and alongside the singing stones and the tree puzzle, before the land's entry scene; no town has to be owned and there is no wait | todo | the land's control script runs it as a background script; that script stops earlier in openblack (see [../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| The temple puzzle (four rings on three columns) is placed on the beach, turned 180 degrees, at normal size | todo | `CREATE_WITH_ANGLE_AND_SCALE` with a puzzle kind does nothing (`CreateScriptObject` in `src/CHLApi.cpp`); the puzzle itself: [../minigames.md](../minigames.md#beach-temple-rings-land-2) |
| A ring of the player's influence, radius 50, is put round the puzzle so the hand can reach the rings although the beach is outside any town; it is never removed, even after the puzzle is solved | todo | `InfluencePosition` is a stub; the call gives no player, so it is the default (neither the "zero" nor the "anti" form) |
| A silver scroll stands about 16 north of the puzzle, raised 2 above the ground; it belongs to this challenge in the challenge list | todo | `CreateHighlight` is a stub; the height is set through the object's altitude property (`SetProperty` works, but there is no scroll to set it on) |
| While the scroll is unclicked, whenever the camera is within 100 of it and the scroll is on screen, the good advisor steps out, points at it and says "There's someone is this little temple. He definitely wants something." (the text table's own typo), at most once every 30 seconds and only when no other scene is using the widescreen | todo | the shared notify loop; `SpiritEject`, `SpiritPointPos`, `RunText`, `GameThingFieldOfView` are stubs |
| The notify loop ends when the scroll or the spot under it is clicked, or when the puzzle is solved first (the quest's end flag is set); either way the scroll is then made active | todo | `GameThingClicked` works in openblack but `SetActive` is a stub; see the quirks below for the end flag |
| The first line can come straight away: the loop's 30-second gap is primed so the advisor may speak as soon as the camera is near | todo | |

## The introduction (scroll clicked)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll (before the puzzle is solved) starts a widescreen scene with the generic script music, and remembers that the introduction was played | todo | `StartCameraControl`, `StartDialogue`, `StartGameSpeed` are stubs; `SetWidescreen` and `StartMusic` work but are never reached |
| The camera glides over 4 seconds to a raised view of the temple; a Greek farmer appears inside the temple (at the puzzle's centre), drawn in full detail, and walks out to a spot just west of it | todo | creating villagers does nothing; `MoveCameraPosition`, `MoveCameraFocus`, `SetHighGraphicsDetail`, `MoveGameThing` are stubs |
| After 2 seconds the camera drops over 3 seconds to ground level beside him; once he has arrived and the camera has stopped, he turns to face the camera | todo | `HasCameraArrived`, `SetFocus` stubs |
| The challenge is recorded in the player's challenge log (the temple's list of challenges) with the title "The Beach Temple Puzzle", a picture of this view, progress 0 and an alignment of -0.5, and a reminder: the good advisor stepping out to say "There is risk of flooding." | todo | `Snapshot` is a stub; why a quest with only a kind outcome is logged leaning evil at the start is not explained anywhere in the source |
| The man: "Thank goodness it's you. You're the only one who can help me." The camera creeps closer over 8 seconds while he plays a talking animation on loop | todo | `RunText`, `TextRead` stubs |
| The man: "My temple here keeps getting flooded during heavy rain." | todo | |
| The man: "The building consists of separate rings which you place over these columns" | todo | |
| The man: "Please, I beg you, move the sections up the beach to rebuild it on the furthest column, where it'll be safe." He turns to the far column up the beach and points once; the camera shifts and tilts slightly up | todo | |
| The man: "But the temple has to be moved correctly. The architect left an explanation on this signpost." This line waits for the player to click on | todo | said "with interaction" |
| A bronze "did you know" signpost appears beside the temple with the rules: "The Beach Temple Puzzle. You must move the Temple to the column furthest from the sea piece by piece. There are four Temple pieces and you may only place a piece on an empty spike or on a wider piece." (filed under miscellaneous tips) | todo | the shared signpost helper; `CreateHighlight` and its text property are stubs; see [../../interface/scrolls_and_signs.md](../../interface/scrolls_and_signs.md) |
| The camera turns to the signpost and the man points at it; once the line is read he faces the camera, talks twice more, and walks back into the temple while the camera returns over 3 seconds to where the player left it | todo | |
| The man fades away, the music stops and the scene ends | todo | `ObjectDelete` stub; `StopMusic` works but is never reached |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Move the four rings, by hand, from their column by the sea onto the column furthest up the beach, never resting a ring on a smaller one | todo | the rules and the solved test: [../minigames.md](../minigames.md#beach-temple-rings-land-2) |
| There is no timer, move limit or penalty; the quest simply waits until the puzzle reports it is solved | todo | `Played` is a stub |
| Clicking the scroll is not needed: the puzzle can be solved before or without the introduction | todo | |

## The flood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing actually floods: the scripts start no water, rain, tide or timer, and the puzzle object in the PC game has no flooding of its own; the flood exists only in the man's lines, the reminder ("There is risk of flooding.") and the script's name | todo | the puzzle class in the executable has setup, process and completion functions for this puzzle but nothing for water or tides |
| The start snapshot records the quest with an alignment of -0.5, as if the challenge leaned evil; the success snapshot then records +1 | todo | `Snapshot` stub; a log entry's record of how well it went and the alignment it earned: [../challenges_and_rewards.md](../challenges_and_rewards.md); the source gives no reason for the -0.5 |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| As soon as the puzzle is solved, the quest's end flag is set (ending the advisor's reminders if the scroll was never clicked) and a widescreen scene starts | todo | |
| The camera glides over 4 seconds to look over the far column; the Greek farmer appears at the rebuilt temple, in full detail, and walks a little west, then faces the camera | todo | |
| If the introduction was played, he says "Thank you, Holy One. No more flooding for me!"; if the player solved it without clicking the scroll, he says instead "You must have noticed my temple was at risk from flooding! Thank you for your actions!" | todo | the choice uses the "introduction played" flag set when the scroll's scene began |
| The man, talking in a loop: "In return for this I will activate the temple's beneficial properties." | todo | |
| The man: "Any living being who comes to the temple will be healed. " (with the text table's trailing space); the dialogue box is then closed | todo | `GameCloseDialogue` stub |
| The camera swings over 4 seconds to look along the temple; he faces the temple and plays the summoning animation (the one the Pied Piper uses) twice | todo | |
| After 2 seconds the challenge log entry is recorded again, with the same title and reminder, progress 1 (complete) and alignment +1 | todo | a fresh `Snapshot`, not an update of the first; `Snapshot` is a stub |
| The camera pulls back over 3 seconds, the healing starts, and the man walks into the temple; 2 seconds after the camera stops, it returns over 4 seconds to where the player left it, the man fades away and the scene ends | todo | |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: there is no time limit, no losing state is tested, and the puzzle cannot be broken (its pieces and columns are put back; see [../minigames.md](../minigames.md#beach-temple-rings-land-2)) | todo | |
| Ignoring the scroll leaves it standing with the advisor's reminders until the puzzle is solved or the land ends | todo | |
| After the introduction, the scroll's script waits until the puzzle is solved; the source's comment says this is to keep the scroll in the world (scripts' own objects are otherwise removed when the script ends, which is why the signpost helper hands its signpost back to the game) | todo | the scroll is never deleted explicitly; it goes when this wait ends at the puzzle's solution (inferred from the comment and the helper's release, not confirmed in the engine) |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A level-2 heal miracle is cast on the rebuilt temple's spot, radius 10, by the scripts (no player's own), and cast again every 20 seconds forever, so any villager, animal or creature within 10 is healed; this lasts for the rest of the land | partial | `SpellAtPos` works in openblack (casts for the neutral player, `src/CHLApi.cpp`), but it is never reached; the cast's time is -1, and whether the engine reads that as endless or as the miracle's normal length is not confirmed here |
| Each new heal is cast without removing the last one; the script keeps only the newest | todo | with an endless time they would pile up; undetermined for the reason above |
| No miracle, dispenser, prayer power or belief is given; the reward is only the healing temple | todo | see [../rewards.md](../rewards.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rules signpost beside the temple is found (within 2 of its spot) and faded away after the success scene | todo | `ObjectDelete` stub |
| If the introduction was never played, no signpost was ever made, so this finds nothing | todo | the signpost is made only inside the introduction scene |
| The rebuilt temple stays on the far column, the influence ring stays round the puzzle, and the healing goes on | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the generic script music from its start to its end | partial | `StartMusic` and `StopMusic` work in openblack (`GameMusic::StartScriptMusic`) but the scene is never reached |
| The success scene starts no music or sound of its own (no reward sting) | todo | |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scripts never look at the player's creature; it can move rings like any other object it picks up only if the puzzle allows it (undetermined: the scripts don't say) | todo | |
| The creature, like any living thing, is healed when within 10 of the rebuilt temple | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's end flag is a game-wide variable shared with other scripts (the land 5 lion puzzle uses it; the land 1 singing stones reset it); the shared helper's own comment warns to use it with caution. On land 2 only this quest uses it | todo | the scroll script resets it to 0 before the notify loop, and the loop resets it again on entry |
| The comments beside the dialogue are older drafts that differ from the spoken text: "Aha. Just the god I was looking for.", "My beach house keeps getting flooded by the tides.", "If you could move it further up the beach I'd be most grateful.", "The signposts provide the rules and instructions you need.", "Cheers, Mighty One. I can live without fear of flooding now.", "In return for your kindess I will activate the powers of the Tower." and "Any living being who visits it will be healed." | todo | the shipped text table has the lines quoted in the rows above |
| Unused values: a timer primed 31 seconds back in the scroll script, and two marked spots in the main script (a start spot and a spot for the man) are set up but never used | todo | confirmed in the compiled program |
| The good advisor's scroll line reads "There's someone is this little temple" (sic) | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Eight of the man's lines, about belongings ruined by a flood, are in the text table but no script says them: "My collection of clocks!", "My soccer trophies!", "The Welsh dresser's toppled!", "Oh, my matchstick galleon!", "My ship in a bottle collection!", "Oh, my hip replacement!", "My CDs!", "My house of cards! That took four years!" | n/a | apparently a cut flooding stage, where the temple flooded and the man lamented as he lost things; flagged differently in the text table from the lines used. Whether the engine itself ever says them is not confirmed: no script source uses them, and a search of the PC executable for their text numbers finds no plausible use |
| Every other line of the quest's set (1 to 8, 17 to 19) is used | n/a | |
