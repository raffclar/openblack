# The Creature Cave

The temple's room for the player's creature. The creature stands in it by a fire, and around the cave are scrolls
telling of its attributes, its personality, the actions it has learnt and the miracles it knows, attack dummies hung
with the belts it has won in fights and plinths with medals for its miracles. Clicking the creature opens the tattoo
editor ([creature_tattoos.md](creature_tattoos.md)). The temple itself is in [../temple](../temple/).

**Progress: 16/31 done, 3 partial — 56%**

## Getting there and moving about

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F5 takes the player into the temple's Creature Cave | done | `CreatureCaveSystem::Update` (into the temple's creature room) |
| Without a temple, as on the testbed, F5 shows the cave's screen on its own, and F5 or Escape closes it | done | `CreatureCaveSystem` |
| The room's camera zooms to whatever is clicked (a scroll, the dummies, the plinths, the creature) and back out | partial | the targets and their zoom points are found as the game finds them (`src/3D/CreatureCaveTargets.*`, `TempleCameraModel.cpp`); zooming onto the creature is a TODO |
| A fourth target's tooltip says it zooms in, but clicking it does nothing | done | `CreatureCaveTargets` (kept as the game has it) |
| The cursor keys turn and tilt the zoomed view, with limits, slowing to a stop | todo | |
| Tooltips say what clicking each target does | done | `src/3D/TempleToolTips.cpp` |
| The way out at the cave's far end leads out of the temple | done | `CreatureCaveTargets` exit target |
| The waterfall's water slides and sounds, and the fire crackles where the creature stands | done | `TempleInterior.cpp` (the waterfall's texture slide and the cave's sounds) |
| Embers drift up from the fire | todo | (unconfirmed what the room's sixteen drifting things are) |
| The scrolls and other room screens close when the player moves the view with the keys | todo | |

## The creature in the cave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature stands in the cave, as it looks in the world (body, skin, tattoos) | todo | the cave draws no creature yet |
| It turns to face the hand as the hand moves about the cave | todo | |
| It looks at the hand with a static pose, at random mirrored, when the hand is in front of it | todo | |
| A click makes it look at where the hand clicked for a second and a half | todo | |
| With no creature yet, the cave is empty and the scrolls say so | partial | the cave screen says there is no creature; the room's own scrolls are not told |
| The cave is always about the player's own creature, whichever the camera was following | done | `CreatureCaveSystem::GetCreature` |
| A picture of the creature can be taken from the cave and saved (unconfirmed what for) | todo | |

## The scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The attributes scroll: name, species, age in years, alignment, strength, size, fatness, health and needs, as whole percentages cut short | done | `creature_cave::FactsOf`, `TempleScrolls`; test `FactsCutPercentagesShort` |
| The personality scroll: each desire it has and how much it likes it, from extremely to not at all, its view of its god and of other creatures | done | test `FactsTellOfTheMind` |
| The actions learnt scroll: what it has learnt to do and not to do, the strongest opinions each way | done | test `LessonsAreTheStrongestOpinionsEachWay` |
| The magic scroll: the skills it has learnt by watching and how far it has learnt each miracle | done | test `FactsTellOfSkillsAndMiracles` |
| The thing it likes most is named and shown | todo | |
| A scroll's text unrolls when zoomed to and rolls up when left | partial | openblack shows the scroll text as a page of its cave screen (`src/Gui/CreatureCaveScreen.cpp`) rather than on the room's scroll |
| The scrolls' pages go round from one to the next | done | test `PagesGoRound` |
| A lesson browser steps through the actions it knows, telling for each what it has learnt about why, on what and how to do it | todo | (unconfirmed how the player opens it) |
| The scrolls' texts come from the game's texts, in the player's language | done | `TempleScrolls::Write` |

## Trophies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Belts hang on the attack dummies by how it fights: a row each way fills, white first, five a colour, as it leans to attack or defence | done | `src/3D/CreatureCaveTrophies.*`, drawn by `TempleInterior::UpdateCaveTrophies` |
| Medals stand on the magic plinths: one for all its miracles together, then its best four, wood to gem by how well learnt | done | `CreatureCaveTrophies` |
| Every belt and every medal past wood is drawn shiny | done | `CreatureCaveTrophies` (environment map) |
| The miracles it knows hover as seeds in the room | todo | (unconfirmed which four the room shows) |
| The fight scroll tells of its fights | todo | (unconfirmed what it lists) |
