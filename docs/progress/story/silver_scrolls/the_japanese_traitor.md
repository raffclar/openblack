# The Japanese Traitor

An untitled Land 5 silver scroll (this file is named after its script, because the game's text table has no title for
it): every evening a stranger, one of Nemesis's own followers, creeps out of the forest to pray at a lantern, and
tapping the silver scroll over him plays a short film in which he reveals what one of Nemesis's three Wonders is doing
to the player's creature. He tells one secret a night, at most three, then is never seen again.

**Land:** 5 · **Giver:** a stranger dressed as a crusader, praying at a fire in the forest about 180 from the neutral Japanese village · **Script:** JapaneseTraitor · **Reward:** none: only information about the creature curse · **Repeatable:** once a night, up to three talks

Sources: the challenge script (the original source text, which matches the PC game's compiled `challenge.chl`), the
land's control script that starts it, the land's creature-curse script, the game's text table and the executable. The
land as a whole is in [../land_5.md](../land_5.md), its script program in
[../../scripts/land5_script.md](../../scripts/land5_script.md). openblack's state is judged on the physics work tree
(`ob-wt-physics`): openblack never runs Land 5's control script (the land-loading native does nothing and the story
always begins with Land 1's control script), so the stranger never appears. Of the 42 script functions the script
needs, 15 work in `src/CHLApi.cpp` (making mobile statics such as the fire and lantern, the indestructible, fire and
pick-up flags, the game time, setting the camera, music, widescreen, distances); the rest, among them making the
villager, walking him, the scroll, timers, dialogue, advisors, camera moves and animations, only log "not
implemented". Every row is todo unless the notes say otherwise.

**Progress: 0/38 done, 4 partial — 5%**

## What the scroll is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is an ordinary silver challenge scroll, but no title for it exists in the game's text table (the 53 scroll titles include nothing about a traitor, the stranger or the Wonders) | todo | `CreateHighlight` is a stub; see [../../interface/scrolls_and_signs.md](../../interface/scrolls_and_signs.md) |
| The script never records the challenge (no start, no finish, no reminder), so it never appears among the challenges with a name, success or alignment, and tapping the scroll again replays nothing | todo | |
| The scroll itself is drawn as the bare silver scroll model with its glint; the game draws no text on any scroll, so in play the player sees just a silver scroll over the praying man | todo | the scroll's hover hint comes from the scroll's kind, not from its challenge (the wording was not resolved) |
| No advisor announces the scroll (the shared "Look. Something for you to do here." notice is switched off); only the forest-fire remarks below lead the player there | todo | |

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts it in the background as Land 5 begins, unconditionally, together with the creature curse | todo | the land's control script never runs in openblack |
| A bonfire is lit at once at the stranger's praying place in the forest | partial | openblack can make the bonfire (`CreateScriptObject` makes mobile statics); the script never runs |
| Everything stops when Nemesis loses his last village (the land then stops every script but its own) | todo | |

## The advisor notices the fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until the stranger has been met, whenever the camera is within 150 of the fire with it on screen, the good advisor comments | todo | `PosFieldOfView` is a stub |
| At night (after 18:00 or before 7:00) the good advisor points at it: "There's something going on down here.", "Someone seems to be hiding out in the forest." and "Let's see what they're up to."; this repeats every 30 seconds while the player keeps looking | todo | `CreateTimer`, `SpiritPointPos`, `RunText` are stubs |
| By day, once only, pointing: "I sense something special about this place." and "Don't forget it. We should come back this way later."; then a minute's pause before any night remark | todo | |
| These remarks stop for good once any of the stranger's talks has begun | todo | |

## The stranger's evenings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each evening, once the time is after 16:00 (or before 8:00), the stranger is made in the woods about 22 from the fire, dressed as a crusader | todo | creating a villager from a script does nothing in openblack |
| He can't be killed, isn't hurt by fire and can't be picked up | partial | the three flags work (`SetIndestructable`, `SetHurtByFire`, `SetIdPickupable`), but the villager isn't made |
| He walks to the fire; when he gets there the bonfire is swapped for a country lantern, and he faces it and prays without end | todo | `MoveGameThing`, `ObjectDelete` are stubs |
| A silver scroll appears at his praying place, as long as he still has a secret to tell | todo | |
| The scroll can be tapped only while it is still evening or night; at 8:00 the scroll goes and he leaves untold | todo | `GameThingClicked` is a stub |
| When he leaves he walks back into the woods; once he is 10 from the lantern it turns back into a bonfire, and when he reaches the woods he fades away | todo | |
| He comes back the next evening, and every evening, until all three secrets are told | todo | |

## The talks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each talk is a film with the second generic script tune; the stranger is drawn in high detail; the camera flies in close to him over 4 seconds and then creeps round him over 8 as he prays (it comes in from a different side each talk) | partial | `StartMusic` and `StopMusic` work; the camera moves and high detail are stubs |
| The first talk only begins with: "You are the true god. This is the time of salvation." and "Your flame burns bright, but there are still shadows." | todo | |
| Every talk then goes on: "Let me help you to light up the whole world!" and "I'd like to tell you more of what I know." | todo | |
| Talk about the Aztec Wonder: "Your Creature is infected by a spiritual curse." and "Its power is greatest when the sun has set."; the camera cuts to the Aztec Wonder: "This Wonder is giving the curse its power.", "While it stands, it will drain your Creature's strength." and "You must destroy it to restore strength to your Creature." as the camera pans across it over 3 seconds | todo | `SetCameraPosition` works; the rest are stubs |
| Talk about the Greek Wonder: the camera cuts to it: "This Wonder is changing the alignment of your Creature, making him the opposite of you." and "It will change your Creature's soul to be like that of Nemesis and the different Creeds will never unite." as it pans | todo | |
| Talk about the Tibetan Wonder: the camera cuts to it: "This Wonder is shrinking your Creature." and "Act too late and your Creature will vanish to nothing." as it pans out over the land | todo | |
| Each talk ends back at the stranger, the camera rising above him over 4 seconds: "I'd like to tell you more of what I know." and "But first I must rest and pray. Please return tomorrow."; the camera then glides back to where the player left it (4 seconds) and he prays again | todo | |
| After a talk he stays praying until the next morning (8:00 to 16:00) and then leaves | todo | |
| The talks go in order (Aztec, Greek, Tibetan); a Wonder already destroyed is skipped, both when he arrives and again when the scroll is tapped | todo | |
| After the third talk he never comes back; if the Wonders he has left to talk about are all gone, he still comes that evening, prays a moment with no scroll and leaves for good; whether the fourth Wonder (in Nemesis's own village) stands makes no difference | todo | |

## What the curse really does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's curse works only at night (before 6:00 or after 18:00), so the stranger's "greatest when the sun has set" is right | todo | the curse script is part of the land's story, see [../land_5.md](../land_5.md); every step: [../gold_scrolls/i_have_a_surprise_for_you.md](../gold_scrolls/i_have_a_surprise_for_you.md) |
| The Aztec Wonder does drain the creature's strength while it stands, so his first warning is right | todo | |
| His other two warnings are swapped: the land's script makes the Greek Wonder shrink the creature and the Tibetan Wonder pull its alignment towards Nemesis's, while the films show the Greek Wonder for alignment and the Tibetan one for shrinking | todo | checked against the curse script's order of Wonders as the land passes them |
| Destroying a Wonder stops its part of the curse; the whole curse lifts when the player holds the three villages, in the land's healing scene, where a crusader-dressed man, whom the land's script treats as the same stranger, has brought the creature to "this place of healing" | todo | the healing scene belongs to the land's story |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each talk plays the second generic script tune and stops it at the end | partial | `StartMusic`/`StopMusic` work; nothing starts the script |
| The stranger's lines are spoken in the standard man's voice | todo | `RunText` is a stub |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature takes no part; the talks only explain the land's creature curse | n/a | nothing to do |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor is never sent home after the forest remarks (the day remark doesn't even stop her pointing); ending the dialogue is all that puts the advisor away | todo | |
| The stranger comes out at 16:00 but the advisor's night remarks only start at 18:00 and stop at 7:00, an hour before he leaves | todo | |
| The "met" flag is set as a talk begins, so an untapped scroll never stops the advisor's night remarks | todo | |
| Only the script's name calls him a traitor; in play he is never named or explained | todo | |

## Cut and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The stranger was first meant to live in a hut near the forest (its spot is written in and switched off) | n/a | never in the game |
| A talk that started when the camera came within 75 (or 50) of him, instead of a scroll tap, is switched off | n/a | never in the game |
| No line or film exists for the fourth Wonder, although the script counts it | n/a | never in the game |
