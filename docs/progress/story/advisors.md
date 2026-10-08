# Advisors and characters

The good and evil advisors, the player's two consciences, who hover at the edges of the screen, fly out into the world,
point, act and talk in the voices of the game's recorded lines, arguing over what the player should do; and the story's
other speaking characters. Which messages the help system sends is in ../interface/help_system.md; this file is who says
them and how. None of it is in openblack yet: no advisor model, voice or dialogue is drawn or played (Diego's bw-clean
branch has a port of the models, motion, text box and voices, not merged).

**Progress: 0/72 done, 0 partial — 0%**

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor: a white-robed, bearded old sage with a halo | todo | its model and 80 clips are in `Data/HelpSprite/markgood.hd`; ported on Diego's bw-clean branch, not in openblack |
| The evil advisor: a small red horned imp | todo | `Data/HelpSprite/markevil.hd`; ported on Diego's bw-clean branch, not in openblack |
| Their clips are layered and additive (a body pose plus face and gesture layers) | todo | ported on Diego's bw-clean branch, not in openblack |
| They are drawn after the scene, in their own view in front of everything, while near the screen | todo | ported on Diego's bw-clean branch, not in openblack |
| Sent out into the world they are drawn in the world instead, blending between the two | todo | ported on Diego's bw-clean branch, not in openblack |
| They fade in at three times a second's alpha, out at twice | todo | ported on Diego's bw-clean branch, not in openblack |
| The good one glows with a halo; both leave a sparkling rainbow trail and a puff of smoke when they appear | todo | ported on Diego's bw-clean branch, not in openblack |
| Their faces blink each eye, and show smiles, frowns, sadness, shock, a raised eyebrow and anger | todo | no advisor code in openblack |
| Their mouths move with their voice: three vowel shapes picked by analysing the sound as it plays | todo | ported on Diego's bw-clean branch, not in openblack (the sound analysis itself not yet) |
| Gestures and emotions while talking come from cue marks in each line's recording | todo | (the cue marks are not read anywhere yet) |
| They have no idle gestures of their own: between lines they just hover | todo | (per Diego's notes) |

## Where they are and how they move

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At rest they sit at the left and right edges of the screen, a few units in front of the camera | todo | ported on Diego's bw-clean branch, not in openblack |
| They drift about on smooth curves, kept apart from each other, from the hand and from the middle of the screen | todo | ported on Diego's bw-clean branch, not in openblack |
| They dodge left, right, up or down out of the hand's way | todo | no advisor code in openblack |
| Appearing and disappearing, they flash in and out | todo | no advisor code in openblack |
| Scripts make them appear and disappear | todo | the script commands are stubs; see ../scripts/ |
| Ejected, an advisor flies out from its corner into the middle of the view in a second | todo | the script commands are stubs; see ../scripts/ |
| Sent home, it flies back to its corner in a second | todo | the script commands are stubs; see ../scripts/ |
| Scripts make one cling to a point on the screen (across, down) | todo | the script commands are stubs; see ../scripts/ |
| Scripts fly one to a place in the world | todo | the script commands are stubs; see ../scripts/ |
| Scripts make one point at a place or an object in the world, and stop | todo | the script commands are stubs; see ../scripts/ |
| Scripts make one point at a place on the screen | todo | the script commands are stubs; see ../scripts/ |
| Scripts make one look at a place, and stop | todo | the script commands are stubs; see ../scripts/ |
| Scripts play an acted animation: nod, shake the head, laugh, cross arms, scratch the head, pick the nose, sulk, cry, dance, pray, punch the air, cover the eyes … | todo | the script commands are stubs; see ../scripts/ |
| Scripts ask whether an advisor has finished its animation | todo | the script commands are stubs; see ../scripts/ |
| While a dialogue runs both advisors come out; for two lines they cling near the bottom, for more they are ejected | todo | the help scripts in the game's challenge file do this; the script commands are stubs; see ../scripts/ |

## Voices and text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every advisor line has its own recording; the good advisor's and evil advisor's lines come from the advisors' bank | todo | see ../audio/voices_and_speech.md |
| An advisor starts speaking up to half a second late, the further it is from its rest place | todo | ported on Diego's bw-clean branch, not in openblack |
| The line shows in a strip above the bottom of the screen, the good advisor's in pale yellow, the evil one's in pink, others in white | todo | see ../interface/on_screen_text.md |
| A line is read when the voice has finished (or, without a voice, after a time by its words and the reading speed) | todo | ported on Diego's bw-clean branch, not in openblack |
| Clicking ends the line being read | todo | ported on Diego's bw-clean branch, not in openblack |
| Six recent lines stay on screen, older ones shrinking and dimming | todo | ported on Diego's bw-clean branch, not in openblack |
| Their voice isn't ducked under the music | todo | (per Diego's notes) |

## What they talk about

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Introducing themselves and teaching the hand and camera in the opening | todo | see tutorial.md |
| Introducing and reminding of challenges and pointing out scrolls | todo | see challenges_and_rewards.md |
| Arguing over each challenge: the good one urges kindness, the evil one cruelty | todo | see the land files |
| Reactions to the player's deeds: killing, destroying, being very good or evil, acting against their alignment | todo | see ../interface/help_system.md |
| Town and battle alerts, winning and losing | todo | see ../interface/help_system.md |
| The creature's learning and what it wants | todo | see ../interface/help_system.md |
| Welcome remarks in the temple and interruptions there | todo | see ../temple/ |
| Rewards and miracle dispensers | todo | see challenges_and_rewards.md |
| The moon's phase on real nights | todo | see ../interface/help_system.md |
| Remarks on the land's towns: what a town wants, food and wood given, a town under attack, disciples made, belief | todo | the guidance remarks; ported on Diego's bw-clean branch, not in openblack; silent on the first land unless marked to always play |
| Rare one-off remarks, once per land, each with a small chance | todo | ported on Diego's bw-clean branch, not in openblack |
| Saying "yes" as the player turns or tilts the camera the way a lesson asks | todo | it never says "no": a slip in the original kept by the port; ported on Diego's bw-clean branch, not in openblack |
| The audio CD left in the drive | n/a | the game's own disc check |

## Idle banter

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the player does nothing for two minutes, the advisors start a short exchange between themselves | todo | the idle time is the time since the player's last use of the interface; no advisor code in openblack |
| Doing nothing means none of the interface events: moving the hand, picking up, catching, throwing, giving, tapping, casting, any gesture, turning, tilting, zooming, double clicking, dragging, the leash, fight moves or asking for help | todo |  |
| The idle clock runs on unpaused game time and stops while the game is paused or a script holds the cinema bars | todo |  |
| An exchange is one of 25 banter sets, picked at random among those not yet heard | todo |  |
| When all 25 have been heard the record is cleared and they come round again | todo |  |
| Starting an exchange counts as the player asking for help, so the next waits another two minutes of idleness | todo |  |
| Banter needs the help system on and a help level above none | todo | see ../interface/options.md |
| It doesn't start while a script's cutscene holds the bars, nor while a challenge script is talking | todo |  |
| A help message already running is stopped for the banter | todo |  |
| For more than two lines both advisors fly out to talk; for two they cling near the bottom of the screen | todo |  |
| Each line is said and waits to be read before the next | todo |  |
| Clicking ends each line; any interface use resets the idle clock | todo |  |
| Multiplayer games have their own banter set of taunts | todo | see ../multiplayer/ |
| Whether banter runs in the temple or with the creature followed | todo | (unconfirmed) |

## Interruptions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A more important message stops the running one | todo | see ../interface/help_system.md |
| Interrupted, the good advisor coughs and the evil one grumbles a bitter remark | todo | no advisor code in openblack |
| Their own lines for being interrupted, and for being interrupted in the temple | todo | no advisor code in openblack |

## Spooky voices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On real nights (by the computer's clock, from a quarter to nine to nine, and from eleven to six) a voice whispers the player's name | todo | ported on Diego's bw-clean branch, not in openblack |
| The name is the player's profile name, matched by how it sounds against 100 recorded names | todo | openblack has no profiles; ported on Diego's bw-clean branch, not in openblack |
| Not on the first two lands | todo | ported on Diego's bw-clean branch, not in openblack |
| The chance rises over time and resets after a whisper; pitch and volume vary at random | todo | ported on Diego's bw-clean branch, not in openblack |

## Other speaking characters

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar, the friendly god of the second land | todo | the script commands are stubs; see ../scripts/ |
| Lethys, who steals the creature | todo | the script commands are stubs; see ../scripts/ |
| Nemesis, the enemy god | todo | the script commands are stubs; see ../scripts/ |
| The creature trainer, the monk, the ogre, the island keeper and the villagers of each challenge | todo | the script commands are stubs; see ../scripts/ |
| A character speaking in a scene turns to face the camera | todo | the script commands are stubs; see ../scripts/ |
| A big booming voice for some lines | todo | the script commands are stubs; see ../scripts/ |
