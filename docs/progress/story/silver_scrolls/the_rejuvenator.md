# The Rejuvenator

A third-land silver scroll that appears once the player wins the Egyptian village: an old woman at a lone hut can turn
old villagers back into children. After she has rejuvenated three, bringing her a child makes her magic go wrong: the
hut blows up and the child becomes an ape creature (a chimp if the player's creature already is an ape), which the
player can swap their creature for. Killing her ends the quest with no creature.

**Land:** 3 · **Giver:** an old woman at a lone Celtic hut, a one-house neutral village · **Script:** SwapToApe · **Reward:** an ape creature to swap to (a chimp if the player's creature is an ape) · **Repeatable:** no (the swap offer itself never ends)

Sources: the quest's script (the original source text, checked line by line against the decompile of the PC game's
compiled `challenge.chl`), Land 3's control, set-up and map scripts, the game's text table and its species list.
openblack is judged on the physics work tree (`ob-wt-physics`): the quest's dialogue, advisor, camera-move, highlight,
challenge-record, timer, special-effect, villager, animation and creature commands are stubs in `src/CHLApi.cpp` (they
only log "not implemented"), `Create` makes neither villagers nor creatures (`CreateScriptObject` only makes mobile
statics and rocks), and `SwapCreature` is a stub. The land-loading command does nothing, so Land 3's control script,
which starts this quest, never runs. Every row is todo unless the notes say otherwise. The land is in
[../land_3.md](../land_3.md); the swap that ends the quest is described in full in [creature_swaps.md](./creature_swaps.md).

**Progress: 0/57 done, 4 partial — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script is started by the land's control script as soon as the player has come through the vortex onto the land | todo | Land 3's control script never runs in openblack |
| It then checks every 10 seconds whether the Egyptian village (Lethys's at the start) belongs to the player; nothing shows until it does | todo | `GetProperty` (a town's player) is a stub; no other condition, so it can appear while the creature is still Lethys's prisoner |
| Once the Egyptian village is the player's, the challenge is registered and a silver scroll appears 4 above the lone Celtic hut, some 440 from the Egyptian village | todo | `CreateHighlight`, `SetProperty` (altitude) are stubs |
| Until the scroll or the hut is clicked, whenever the camera is within 100 of the scroll and it is on screen, the evil advisor steps out at most every 30 seconds, points at it and says "A Silver Reward Scroll. Let's see what it's all about." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll or the hut makes the scroll active and the quest goes on; no time limit before that | todo | `GameThingClicked` works, `SetActive` is a stub |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An old woman (a Norse housewife) comes out of the hut | todo | `Create` makes no villagers |
| A cut scene with the "spooky" script music starts; the camera glides up and out to look at her (3 seconds) | partial | the music command plays the track (`StartMusic`, `src/Audio/GameMusic.cpp`); the scene never runs |
| After 2 seconds the camera moves to a low view in front of the hut (4 seconds) as she walks to a spot about 4 in front of the door | todo | `MoveCameraPosition`, `MoveGameThing` stubs |
| When she is there the camera centres on her head (3 seconds) and she turns to face it | todo | |
| The challenge record opens: title "The Rejuvenator", progress 0, reminder "This woman is giving people the power of youth." (good advisor) | todo | `Snapshot` is a stub |
| She gossips (animation) as the camera closes right in over 8 seconds: "Oooh 'Ello. Pleased to meet you. I can make the old young." | todo | `PlayAnimation`, `RunText` stubs |
| A second gossip animation: "Bring me an old Villager and I'll wind back the years. Oh yes." (spoken in the game's "with interaction" form; undetermined what that changes for the player) | todo | |
| The camera goes back to where the player had it (5 seconds for the position, 4 for the focus) while she starts wandering around her spot, within 6 of it | todo | `SetScriptState` (wander) is a stub |
| Good advisor: "I don't like this old bat at all."; evil advisor: "I say we try her out. It might hurt people." | todo | |
| The music stops and the scene ends | todo | |

## Rejuvenating three villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player brings villagers to her, normally by carrying them in the hand and putting them down near the hut | todo | carrying villagers works in openblack's hand; the quest's checks don't |
| Every 3 seconds the quest picks one villager within 20 of the hut that no script is using | todo | `CreateTimer`, the radius search are stubs; undetermined which villager is picked when there are several |
| It acts only when that villager is not in the hand and has more than 5% health, and the woman is not in the hand and is within 20 of the hut | todo | |
| A villager aged 13 or younger: the woman says "They're not old enough. Hardly worth the bother." and the quest waits until that villager is more than 20 from the hut, or 20 seconds | todo | |
| An older villager is rejuvenated in a cut scene: the camera cuts to a view by the hut and creeps sideways over 10 seconds | partial | the camera cut works (`SetCameraPosition`/`Focus` in `src/CHLApi.cpp`); the creep and the scene don't |
| The villager and the woman both walk very slowly (speed 0.2) to the hut's door | todo | |
| When both are within 1 of it: a spell-success sparkle for 4 seconds, a light camera shake (radius 20, strength 0.1, 1 second), and the villager becomes four years old and loses their job | todo | `SetProperty` (age), `SetDisciple`, `CameraShake` stubs |
| A second later the child walks to the woman's spot in front of the hut; when the camera has finished its move it cuts back to where the player had it, and the woman wanders again | partial | the camera cut back works; the rest doesn't |
| The child is then let go to live as a normal villager | todo | `ReleaseFromScript` is a stub |
| After three villagers are rejuvenated the woman says "Phew. All this work has made me feel dizzy." and the record moves to half done | todo | `UpdateSnapshot` is a stub |
| There is no time limit; villagers of any tribe count | todo | read from the script |

## Hut and woman rules (both stages)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the hut is damaged (its health drops below what it was when she came out) and the camera is within 100 with the hut on screen, she says "Oh come on! Leave my hut alone!"; this is checked only once, so damage seen from far away uses it up silently | todo | |
| If the woman is thrown, while she flies the camera follows her and the game runs at half speed, until she lands | todo | `SetGameSpeed` and camera follow are stubs |
| The script never sets the game speed back to normal after her flight (undetermined whether ending the scene restores it) | todo | |
| While she is in the hand, or more than 20 from the hut, nothing happens | todo | |

## The transformation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No hint is given: the next step is to bring a child; every 3 seconds the quest picks a child within 20 of the hut that no script is using, and acts if it is alive, the woman isn't in the hand and is within 20 of the hut | todo | the evil advisor's hint "We should bring her a kid, Boss. See what happens." is commented out as giving it away |
| The three children she just made are let go beside her and count, so the transformation can follow at once without the player bringing anyone | todo | read from the script (they are freed at her spot, 4 from the hut, inside the 20 radius) |
| A cut scene: the woman is drawn in high detail, sound effects are switched on, the camera cuts to a view of the hut and both walk to its door | partial | the cut works (`SetCameraPosition`/`Focus`) |
| When the child is within 1 of the door: the short epic sting plays, the camera shakes (4 seconds) and a spell-success sparkle covers the child for 5 seconds | todo | |
| A second later the woman walks back to her spot (speed 0.4); after 2 seconds she turns to the hut, scared stiff, and the hut explodes and is gone | todo | `Delete` (with explosion) is a stub |
| Half a second later an ape creature appears where the child stood, or a chimp if the player's creature is already an ape; the child is removed | todo | `Create` makes no creatures; openblack can build both species (`CreatureArchetype::Create`), but its species list numbers the ape differently from the scripts (the scripts' ape is number 0, which openblack calls Unknown; openblack's ape is its "Giant Ape") |
| She turns to the creature, scared stiff again; a second later the record closes as a success (progress 1, alignment 0.3 towards evil) and its reminder becomes "But it's a special Creature. We can swap our Creature to it." | todo | |
| The woman: "Oops. I've created a Creature. Sorry. What a faux pas." and she walks off towards the Egyptian village | todo | |
| Good advisor: "But it's a special Creature. We can swap our Creature to it."; evil advisor: "Yeah? I suppose we bring our boy over and click on it." (the last token is the action-button picture); good advisor: "Precisely." | todo | |
| The camera cuts back to where the player had it, the music stops and the scene ends; the quest's scroll is removed | todo | |
| The woman is removed the moment she is off screen | todo | |

## The swap offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears over the new creature, which keeps turning to look at the camera; when the camera is near, the evil advisor says "Swap your Creature with this one if you want." at most once a minute | todo | the shared swap script; full detail in [creature_swaps.md](./creature_swaps.md) |
| Clicking it with the player's creature further than 50 away: "You'll need to bring our Creature, Boss." | todo | |
| Otherwise the two creatures meet, the good advisor asks to confirm and clicking the new creature confirms, clicking the player's own cancels | todo | |
| On confirmation the creature's mind moves into the ape (or chimp) in a short scene, and the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | todo | `SwapCreature` is a stub; openblack has no way to change the player's creature's species or move its mind into another body |
| The old body stays under a scroll and the offer never ends, so the player can swap back and forth | todo | |

## Killing the woman

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the woman dies at any point before the transformation, the quest ends: the record closes as finished (progress 1) with alignment fully evil, and no creature is made | todo | |
| Evil advisor: "You killed her. Don't feel bad. I'd have done the same. Ha."; good advisor: "Oh when will the horror ever end? Don't answer that, evil one." | todo | |
| The scroll over the hut is not removed in this case (undetermined whether the game removes it when the script ends) | todo | |

## Aftermath, advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut is destroyed for good; the rejuvenated villagers stay children and grow up normally | todo | |
| The reminder line, replayed by clicking the scroll, is spoken by the good advisor | todo | the shared reminder script |
| Music: the "spooky" script track for the introduction, the short epic sting for the transformation | todo | music itself works (`StartMusic`); counted in the scene rows |
| There is no reward chest; the creature is the reward | todo | |
| The player's creature only matters for its species (ape gives a chimp) and for the swap | todo | |

## Bugs, quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rejuvenated child is let go near the hut, so the next check of the first stage can pick it again and the woman says "They're not old enough. Hardly worth the bother.", pausing the quest up to 20 seconds (undetermined whether the villager search also returns children) | todo | read from the script |
| The opening record line was first written before the cut scene and moved inside it; a closing record line after the swap is commented out | n/a | source history only |
| The text table has no lines numbered 1, 6 or 7 for this quest, and line 17 (the evil advisor's hint) exists but is never said | n/a | cut material |
| The source creates a plain "female" villager; the compiled game turns that into a Norse housewife | n/a | data detail |
| The record's alignment is mildly evil for a successful transformation and fully evil for killing her | todo | `Snapshot` is a stub |
| A saved game keeps the stage, the woman and the offer | todo | openblack has no saved games |
