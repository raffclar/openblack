# Swap To Brown Bear

A fifth-land silver scroll that appears once the player wins the neutral Japanese village: a villager complains of a
stench from the forest, and the player follows a trail of eight piles of dung, picking each one up, to find the brown
bear that made them. The bear stays as a creature the player can swap their own creature for, and a heal miracle chest
falls beside it.

**Land:** 5 · **Giver:** a Japanese farmer from a house on the edge of the neutral Japanese village · **Script:** SwapToBrownBear · **Reward:** a brown bear creature to swap to, and a heal miracle chest given to the player's home village · **Repeatable:** no (the swap offer itself never ends)

Sources: the quest's script (the original source text, checked line by line against the decompile of the PC game's
compiled `challenge.chl`, where it is compiled), Land 5's control script and map script, the game's text table and its
reward table (`info.dat`). openblack is judged on the physics work tree (`ob-wt-physics`): the quest's dialogue,
advisor, camera-move, highlight, challenge-record, special-effect, villager and creature commands are stubs in
`src/CHLApi.cpp` (they only log "not implemented"), `Create` makes neither villagers, dung nor creatures
(`CreateScriptObject` only makes mobile statics and rocks), and `SwapCreature` is a stub. The land-loading command does
nothing, so Land 5's control script, which starts this quest, never runs at all. Every row is todo unless the notes
say otherwise. The land is in [../land_5.md](../land_5.md); the swap that ends the quest is described in full in
[creature_swaps.md](./creature_swaps.md).

**Progress: 0/53 done, 2 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is started by the land's control script the first time the neutral Japanese village becomes the player's, however it was won | todo | Land 5's control script never runs in openblack; `GetProperty` (a town's player) is a stub |
| It is started only once: losing the village to Nemesis and winning it back does not start it again | todo | |
| The check sits in the land's main loop that runs while the creature is cursed, until the three wonder villages (Greek, Tibetan and Aztec) are all the player's; if the player takes all three before the Japanese village, the loop ends and this quest is never offered | todo | read from the control script: no later code starts it; the curse: [../gold_scrolls/i_have_a_surprise_for_you.md](../gold_scrolls/i_have_a_surprise_for_you.md) |
| The control script's town checks run one after another with the cut scenes for taking the wonder villages, so the scroll can appear a little after the village is won if one of those scenes is playing | todo | |
| As soon as the quest starts, a silver scroll appears over a house on the village's edge (about 60 from its centre) and the challenge is registered | todo | `CreateHighlight` is a stub |
| Until the scroll or the house is clicked, whenever the camera is within 100 of the scroll and it is on screen, the good advisor steps out at most every 30 seconds, points at it and says "We've got something to do. Let's see what it is." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll or the house makes the scroll active and the quest goes on; nothing happens before that click (no time limit) | todo | `GameThingClicked` works (see [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md)), `SetActive` is a stub |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first pile of dung appears at once, about 100 west of the house, with a swarm of flies over it that lasts for ever | todo | `Create` makes no dung; `CreateSpecialEffect` is a stub |
| A Japanese farmer comes out of the house (the source notes he was a woman until the voice used turned out to be a man's) | todo | `Create` makes no villagers |
| A cut scene with the "happy" script music starts; the farmer is drawn in high detail | partial | the music command plays the track (`StartMusic`, `src/Audio/GameMusic.cpp`); the scene never runs |
| The camera glides to a low view by the house (3 seconds for the position, 4 for the focus) | todo | `MoveCameraPosition`/`Focus` are stubs |
| After 2 seconds the farmer walks slowly (speed 0.4) to a spot a few paces in front of the house, then turns to face the camera | todo | `MoveGameThing`, `SetFocus` on villagers are stubs |
| He plays his "poisoned" (retching) animation once while the camera creeps closer over 12 seconds | todo | `PlayAnimation` is a stub |
| A quarter of a second after it ends, the challenge record opens: title "Swap To Brown Bear", progress 0, reminder "The people here are complaining about the stench." (good advisor) | todo | `Snapshot` is a stub |
| He looks around as if searching and says "Ugh. There's a frightful stench coming from the forest." | todo | |
| He turns to the first pile of dung, talks and points, as the camera swings up and round to look west over the forest (3 seconds): "Could you check it out, big one. It's sickening." | todo | |
| The camera goes back to where the player had it, but now looking at the first pile of dung (3 seconds) | todo | |
| The farmer walks back to his house, the music stops and the scene ends; once he is within 2 of the house he is removed | todo | |
| Straight after, the good advisor steps out: "Sickening, is it? I don't believe you." then "Hmm. Yes it really is rancid. I wonder what's causing it?" (the evil advisor is popped out for the second line, but both lines are recorded in the good advisor's voice) | todo | |

## Following the trail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player clears the dung by picking each pile up with the hand; the moment a pile is in the hand it vanishes (the hand is left empty) and the next pile appears with its own flies | todo | `GetProperty` (in hand) and `Delete` are stubs |
| Only one pile exists at a time; they lead from near the village west, then north through the forest, each 25 to 45 apart: eight piles in all, the last about 220 north-west of the house | todo | positions from the script |
| After the first pile, the good advisor: "Ugh. I can still smell something. There must be several more around. Let's clear them up." | todo | |
| After the seventh pile, the evil advisor: "The smell is almost gone. There's one more out there, though." | todo | |
| Picking up the eighth pile ends the trail | todo | |
| The flies of a picked-up pile are never removed by the script (undetermined whether they go with the pile) | todo | |
| There is no time limit, no counter on screen and no way to fail; the creature can't do it for the player (only the hand holding a pile counts) | todo | read from the script; whether a creature picking a pile up counts as "in hand" is undetermined |
| If the farmer's house is damaged at any time during the trail (its health drops below what it was after the introduction), the good advisor steps out, points at it and says "The owner of that house will be a little distraught." (once only) | todo | |
| A further hint was meant for the first pile: once it was on screen with the camera within 100, the good advisor would point at it and say "We really should clean up this mess. Someone could step in it." It never plays (see bugs) | todo | script behaviour to keep |

## Finding the bear

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell-success sparkle appears for 2 seconds at a spot about 30 past the last pile, almost at sea level, and a brown bear creature appears there | todo | `Create` makes no creatures; openblack can build a brown bear with `CreatureArchetype::Create` (used by the dev creature spawner) |
| A cut scene with the short epic sting starts; the camera moves to a low view of the spot over 2 seconds | partial | `StartMusic` plays the sting; the scene never runs |
| The good advisor: "Aha! So that's what's been causing this smell!" | todo | |
| The bear walks about 25 further north, the camera following it down over 2 seconds; when it is there the camera drops lower | todo | |
| The heal miracle chest falls from the sky onto the spot the bear appeared on (see below) | todo | |
| The evil advisor: "The brown bear. So they do do it in the woods!" while the bear looks back at the spot, for at least 3 seconds | todo | |
| The bear turns to the camera, both advisors go home, the dialogue closes and a second later the challenge record closes as a success (progress 1, alignment 0) | todo | |
| The music stops and the scene ends; the bear is then handed to the shared creature-swap offer | todo | |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest drops from the sky at the bear's spot, given in the player's home Norse village (the first village on the land), without its own camera film | todo | `CreateRewardInTown` is a stub; see [../rewards.md](../rewards.md) |
| The chest is the reward table's first heal kind: it holds a heal miracle seed (undetermined whether it is the powered-up heal its name suggests; the table's power-up field is empty for both heal kinds) | todo | read from the reward table in `info.dat` with openblack's `GRewardInfo` layout |
| Once the chest is opened, the good advisor says "Great! We can heal people with this." if no dialogue is in the way; if this were the game's first reward, the first-reward explanation would play instead | todo | the shared reward script |
| The real reward is the bear itself: a new species for the player's creature, if they want it | todo | see the next section |

## The swap offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears over the bear and it keeps turning to look at the camera; when the camera is near, the evil advisor says "Swap your Creature with this one if you want." at most once a minute | todo | the shared swap script; full detail in [creature_swaps.md](./creature_swaps.md) |
| Clicking it with the player's creature further than 50 away: "You'll need to bring our Creature, Boss." | todo | |
| Otherwise the two creatures meet, the good advisor asks to confirm ("Are you sure you want a new Creature? ...") and clicking the bear confirms, clicking the player's own creature cancels | todo | |
| On confirmation the creature's mind moves into the brown bear in a short scene, and the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | todo | `SwapCreature` is a stub; openblack has no way to change a creature's species or move its mind into another body |
| The old body stays under a scroll and the offer never ends, so the player can swap back and forth | todo | |

## Aftermath, advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The farmer is gone for good (removed once home); the house stays | todo | |
| The scroll over the house is never removed by the script (undetermined whether the game removes it when the quest's script ends) | todo | |
| The reminder line, replayed by clicking the scroll, is spoken by whichever advisor owns it (the good advisor) | todo | the shared reminder script |
| Music: the "happy" script track for the introduction, the short epic sting for the bear | todo | music itself works (`StartMusic`); counted in the scene rows above |
| The player's creature is not involved until the swap; nothing checks the creature during the trail | todo | |

## Bugs, quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pointing hint for the first pile ("We really should clean up this mess. Someone could step in it.") can never play: the step counter is set past it just before the trail loop starts | todo | dead branch, the same in the compiled program |
| The source's to-do notes say the trail was meant to lead "through a forest to reach the brown bear" and the camera was meant to "zoom to bear"; the shipped version does both only roughly as described above | n/a | development notes, nothing to do |
| The text table has no lines numbered 5, 8 to 11 or 16 for this quest, so lines were cut; the source's comments quote older wordings of three lines ("I don't think the owner of that house is gonna be too pleased.", "Urgh. The smell still persists...", "The smell has improved drastically...") | n/a | the shipped wordings are the ones above |
| The quest is the only one on the land tied to winning the Japanese village, and can be missed for good by taking the wonder villages first | todo | see "How it appears" |
| A saved game keeps where the trail is, the bear and the offer | todo | openblack has no saved games |
