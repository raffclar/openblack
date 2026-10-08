# Khazar's Shield Challenge

A land 2 gold scroll and Khazar's lesson in defence: at a lone hut on a small island off the east shore Khazar casts a
Physical Shield and lets the player throw rocks at it, then hands over three shield seeds; the player shields the hut,
Khazar drops three boulders on it, and whatever happens the Physical Shield miracle is given to the player's village.
It is started, together with [Khazar's Fireball Challenge](khazars_fireball_challenge.md), by the "Miracle Challenge"
scroll Khazar leaves after the [Worship Site](worship_site.md) lesson (that scroll and Khazar's tour are described in
the fireball file). The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), the miracle itself in
[../../miracles/physical_shield.md](../../miracles/physical_shield.md).

**Land:** 2 · **Giver:** Khazar (the friendly god), through a scroll on the island hut · **Script:** PhysicalShieldChallenge · **Reward:** the Physical Shield miracle at the player's home village's worship site, given whatever the result · **Repeatable:** no

Sources: the quest's script source (`Land2ShieldChallenge.txt`), the scroll that starts it (`Land2FireballChallenge.txt`),
the land's scroll-notify helper (`SetupLand2.txt`), the hand demo (`HandDemos.txt`) and the land's control script,
checked against the compiled `challenge.chl`; the game's English text table; and the land's map script
(`Scripts/Land2.txt`) for the hut. The attack on the hut is Khazar's own boulders, not Lethys's (the land file's summary
row says Lethys). openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 53 script functions
this challenge needs, 38 still only log "not implemented" in `src/CHLApi.cpp` (the scroll, dialogue, advisors,
Khazar's hand, the moving camera, timers, the held object, finding a cast miracle, object properties, aiming a rock,
the hand demo, the challenge log), and the land's control script never runs in openblack (the land-loading function
does nothing and the story always begins with Land 1's script), so the quest never appears; every row below is todo
unless the notes say otherwise.

**Progress: 0/61 done, 11 partial — 9%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Started only by clicking the "Miracle Challenge" scroll by the worship site, at the same moment as the fireball challenge; the two can be done in either order | todo | see [khazars_fireball_challenge.md](khazars_fireball_challenge.md#the-miracle-challenge-scroll) |
| If Khazar has died before the Miracle Challenge scroll is clicked, this challenge is never started | todo | |
| Nothing in the land waits for it: no flag is set on completion | todo | |
| What it unlocks: only the Physical Shield miracle at the home village's worship site; the same miracle also comes with Town2 (Khazar's Greek town at the start) and Lethys's home town when they are won | todo | `Land2.txt` gives towns 2 and 5 a physical shield miracle |
| While it runs Khazar counts as busy, which holds back his death and so the end of the land (see Script quirks) | todo | |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut is the single house of a tiny empty Norse village on an island, a neutral village the map makes uninhabitable for the challenge | partial | the house is made by openblack's map loading (`AbodeArchetype`); the uninhabitable flag is not |
| A gold scroll appears on the hut | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and the scroll is on screen, the good advisor steps out, points at it and says "Here's one of Khazar's lessons. It's the Shield. We've got to try it, Leader.", at most once every 30 seconds and only outside films | todo | the land's own scroll-notify loop; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| The quest waits for the scroll or the hut to be clicked; the scroll is then switched to its active look | todo | `GameThingClicked`, `SetActive` are stubs |
| If Khazar dies before the click, the scroll is made active and nothing more happens | todo | |

## The introduction and Khazar's demonstration

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click Khazar counts as busy and his hand flies fast to a spot on the shore south-west of the hut | todo | `MoveComputerPlayerPosition` is a stub |
| A ring of influence of radius 40 is made round the hut, and the hut is mended to full health | todo | `InfluencePosition` and setting health are stubs |
| A film starts with Khazar's music; the camera moves in on the hut over 3 seconds | partial | `StartMusic` works in openblack; the camera moves don't |
| The log entry "Khazar's Shield Challenge" is recorded at 0 with the reminder "We need to throw stones at Khazar's Shield" | todo | `Snapshot` is a stub |
| Khazar: "Soon you will need to protect your Villagers and their buildings."; the camera slides round over 6 seconds | todo | `RunText` is a stub |
| Three rocks (0.7 scale) appear on the mainland about 80 m west of the hut, 2 m apart | partial | `CreateWithAngleAndScale` makes rocks in openblack; never reached |
| Khazar: "The Physical Shield protects against objects which are not of a spiritual nature." and "When you have the Miracle, this is how you cast a Shield." | todo | |
| Whatever the player is holding is deleted, so the hand is empty for the demo | todo | `ObjectDelete` is a stub |
| Khazar's shield hand demo: the camera frames the hut over 2 seconds and a shield miracle is put in the hand with seven times a normal shield's strength, "to deflect the 3 rocks"; Khazar: "You move the Hand to where you wish to start the Shield.", "Then you hold down the Action Button and trace the perimeter of the Shield. Like this.", "And the Shield appears." as the demo hand draws the circle and the shield goes up over the hut | todo | `PlayHandDemo` is a stub; openblack has no hand demos |
| The camera moves over 2 seconds to a spot by the rocks, aimed at the hut, and the player's control is cut down to the hand | partial | `SetInterfaceInteraction` works in openblack; never reached |
| As the film ends Khazar says "Now throw these rocks at the hut and watch the Shield." | todo | |

## Throwing the rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest finds the shield within 5 of the hut and waits until the shield is gone or hit, or 20 seconds pass | todo | `GameThingHit`, finding a cast miracle are stubs |
| The shield stopping thrown rocks (and wearing down) is the miracle's normal behaviour | partial | the physical shield works in openblack ([../../miracles/physical_shield.md](../../miracles/physical_shield.md)); the challenge never runs |
| Full control comes back after the wait | partial | `SetInterfaceInteraction` works; never reached |
| If the shield was hit or broke: a film, the camera rising over 6 seconds to look down at the hut; Khazar: "See? The hut is protected. But a strong enough attack will destroy the Shield." then "Now you can try. Shield the hut and I will attack it." | todo | |
| If 20 seconds pass with no hit: Khazar says "The Shield deflects the rocks." (whether or not any rock was thrown) as the camera rises, then "Now you can try. Shield the hut and I will attack it." | todo | |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three Physical Shield one-shot seeds appear on the shore about 40 m south-west of the hut, 2 m apart, each with three times a normal shield's strength | todo | creating one-shot seeds is not done by `CreateScriptObject`; `SetProperty` is a stub |
| Khazar's demonstration shield is removed and the hut mended to full health again | todo | |
| The player's control is cut down to the hand | partial | `SetInterfaceInteraction` works; never reached |
| The log entry moves to 0.3 with the new reminder "We need to cast a Shield over the hut on this island." | todo | |
| The quest waits until the player holds a shield seed or a shield stands within 5 of the hut; every 30 seconds (the first at once) the good advisor says "Tap the Shield Miracle and wait." | todo | `GetObjectHeld`, `IsOfType`, timers are stubs |
| Lost seeds come back: about every sixth pass, a seed not held that is off screen or more than 12 m from the first seed's spot fades out and is made again at its own spot | todo | |
| Once a seed is held, the evil advisor says "Hold the Action Button and draw a circle on the ground to place the Shield." | todo | |
| The player then gets one go: the quest waits 1 second, looks for a shield within 5 of the hut, and if there isn't one waits until the hand is empty and 3 seconds more, then moves on whether or not a shield was cast | todo | see Script quirks |
| A shield drawn so that its centre is within 5 of the hut counts; a shield beside the hut does not | todo | |
| If a shield is found, the good advisor steps out: "Hey! You've cast the Miracle! Marvellous!" | todo | |
| Full control comes back | partial | `SetInterfaceInteraction` works; never reached |

## Khazar's attack (the test)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the hut is mended to full health again and Khazar's hand rises to 20 m above the hut; the camera moves to the shore over 4 seconds, looking up at it | todo | |
| Khazar: "The Shield is ready." (said whether or not the player cast one) | todo | |
| Three boulders, each a random 0.6 to 0.8 scale, appear 50 m above the hut at random spots up to 25 m away and are each aimed to land on the hut within 2 seconds, half a second apart | partial | `CreateWithAngleAndScale` makes the rocks in openblack; aiming them (`SetTarget`) is a stub |
| Three seconds later Khazar's hand goes back to the shore | todo | |

## Success and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera closes on the hut over 3 seconds and the log entry is set to 1 either way | todo | |
| If the hut is still at full health (any damage at all counts as a failure): Khazar says "The hut survived my onslaught. Well done.", "Different sized objects do different levels of damage to a Shield." and "It'll disappear after taking excessive damage as well." | todo | building health reads are stubs |
| Otherwise: "You failed to Shield the hut. It is destroyed." and "It is no disaster this time. However you will need practice further." | todo | |
| There is no retry and no penalty: both endings go on to the reward | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera pulls back over 4 seconds and the Physical Shield miracle is given to the player's home village, so its worship site can pray for it | todo | the command that adds a miracle to a town is a stub; see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Khazar: "I have given the knowledge of the Physical Shield Miracle to your Village." and "From now on your people can worship for this Miracle at your Worship Site." | todo | |
| The music stops, the film ends, Khazar is released and no longer busy, and any shield seeds left fade away | partial | `StopMusic` works; the rest are stubs |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut stays as damaged as the boulders left it; the boulders and the three practice rocks stay on the land | todo | the script never deletes them |
| The influence ring round the hut is never removed by the script (undetermined: whether it goes when the script ends) | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor owns the notification, the log reminders, the tap tip and the praise; the evil advisor gives the drawing tip | todo | `SpiritEject`, `RunText` stubs |
| Clicking the log entry speaks its current reminder through the good advisor, who steps out to say it | todo | the shared reminder script |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's script music plays from the click until the reward | partial | `StartMusic`/`StopMusic` work in openblack; never reached |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; nothing in the scripts reacts to it (undetermined what happens if it picks up the seeds or rocks) | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The loop meant to give the player time and fresh seeds counts the remaining seeds into a total it never adds to, so it always believes the seeds are used up: the player gets exactly one attempt, ending 3 seconds after the hand is next empty | todo | the "out of seeds" branch is taken on the first pass |
| The third seed's respawn check is guarded by whether the first seed exists instead of the third (both waits), so with the first seed gone the third is never replaced, and with the first present a used third seed may be remade (undetermined which happens in the engine) | todo | |
| The seeds' spot is about 40.6 m from the hut, just outside the radius-40 influence ring, with one seed inside it and one outside (undetermined whether the player can reach all three without the home village's influence) | todo | measured from the script's markers |
| Holding any object (a villager, a rock) when the demonstration starts deletes it | todo | the script's own comment: "If so delete it" |
| Khazar counts as busy from the click to the reward; a challenge clicked and left half done (seeds never taken) keeps him busy, which holds back Khazar's death and so the rest of the land | todo | the land's control script only starts his death when he isn't busy |
| The busy flag is shared with the fireball challenge and Khazar's other lessons, so whichever finishes first clears it while this one may still be running | todo | |
| Khazar's "The Shield is ready." is spoken even when no shield was cast | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand demo's line "Make sure you join up with the start position." is cut; the script's comment says the speech was too slow for the demo | n/a | in the text table, commented out in the demo |
| Unused retry lines in the text table: "I give you more Seeds to practice with.", "You were unsuccessful. Try again.", "Try recasting the Shield Miracle." and "Draw a circle with the Action Button pressed to mark the position and size of the Shield.", which fit the multi-attempt loop the shipped script never gives; two more entries are marked "NOT USED" | n/a | never spoken by the shipped script |
| A commented-out timer that would have dropped the boulders after 30 seconds whatever the player did | n/a | left in the source as comments |
| A commented-out final log update at the end | n/a | |
