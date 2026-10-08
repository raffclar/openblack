# Pointing and tooltips

What the hand tells the player about what it is over: the word, mouse button and arrows it shows by the cursor ("Pick Up",
"Throw", "Rotate" …) and the numbers it shows over buildings, towns and the creature. How tooltips fade, their levels and
their look belong to [../interface/](../interface/); this file covers which tooltip the hand shows where.

**Progress: 7/49 done, 1 partial — 15%**

## How the hand picks its tooltip

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each turn the hand submits the tooltip for what it is over or holding, with the mouse button and arrows that do it | partial | the machinery is done (`src/Gui/ToolTips.cpp`; tests `ToolTips.*`); the world submits only the creature's |
| A tooltip of a number fills the number into its text, rounded to a whole | todo | |
| A tooltip with no button to show is shown as words only | done | `ToolTipAction` none |
| A shown tooltip lingers for its display time after the hand moves off, if its info says so | done | test `ToolTips.LingerAfterFocusWhenTheirInfoSaysSo` |
| The hand over a debug window shows no tooltip | done | `Game::UpdateHandInterface` (openblack only) |
| No tooltips while a script's cinematic has the interface or the menu is open | done | `Game::ProcessHandToolTipTurn` |
| Showing a tooltip triggers the help system's matching help event | todo | see [../interface/](../interface/) |

## Over things in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the player's own creature, awake: "Interact" | done | `Game::ProcessHandToolTipTurn`, `creature_hand::ShowsInteractTip` |
| Over something it can take: "Pick Up" or "Grab" | todo | The hand can take things now (`HandGrabSystem`); the tooltip isn't shown |
| Over something flying: "Catch" | todo | |
| Holding something: "Throw", "Drop" | todo | |
| Holding something over the creature: "Give" | todo | |
| Holding resources over a worship site or workshop: "Supply"; a villager over a worship site: "Sacrifice" | todo | |
| Holding a tree: "Plant"; wood over a building site: "Build"; a scaffold over another: "Combine Scaffolds" | todo | |
| Over things that can be tapped: "Tap", "Tap To Break", "Tap To Open", "Activate" | todo | |
| Over the player's temple entrance: "Enter Temple" | todo | the entrance works but shows no tooltip |
| Over a signpost: "Signpost"; over a scroll: "Scroll" | todo | |
| In a creature fight: "Fight", "Attack", "Block", "Stop" | todo | see [../creature/](../creature/) |
| Over a building, its name (Abode, Village Store, Crèche, Workshop, Wonder, Graveyard, Village Centre, Football Pitch, Field, Totem, Miracle Dispenser) | todo | the football pitch's: [../town/football.md](../town/football.md) |

## Numbers over things

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a village store: "Stored - Food: … Wood: …" | todo | |
| Over a store or pot: "Food Amount: …" or "Wood Amount: …" | todo | |
| Over a pile or other amount: "Amount: …" | todo | |
| Over a village centre: "Population: …" followed by its total in all | todo | |
| Over a village centre: "People Worshipping: …%" | todo | |
| Over a worship site: "Prayer Energy: …" | todo | |
| Over the temple: "Temple Health: …%", its health times 100 | todo | |
| Over a scaffold: "Scaffold Value …" | todo | |
| Over a building site: "Wood Required: …", or "Building Completed: …%" once no wood is needed | todo | |
| Over the workshop making something: "Production Completed: …%", the part made of the current scaffold | todo | |
| Over a town's desire flag: "Need: …%", the town's raw desire capped at 100% | todo | see [../town/](../town/) |
| Over a town: its desires as percentages (food, wood, play, protection, mercy, expansion, civic building, worship supply, children, building, rain, sun, repair, workshop supply, wonder, relaxation, sleep, rest) | todo | see [../town/](../town/) |
| Over a town not yet won: "Belief Left", the belief still to win times 1000, or "Belief Needed" when it is below nothing | todo | see [../worship/](../worship/) |
| Over a town: "Got … Belief" | todo | |
| Over a worship icon charging: the miracle's name and its charge as a percentage, its name alone when uncharged | todo | see [../miracles/](../miracles/) |
| Over some things: their food and rest desires as percentages | todo | (unconfirmed which objects) |

## The leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the leash on the hand and nothing under it: "Focus Creature" | todo | see [../gesture/leash_gestures.md](../gesture/leash_gestures.md) |
| With the leash on the hand over the thing it is tied to: "Detach Leash" | todo | |
| With the leash on the hand over something else it can be tied to: "Attach Leash", once the creature has grown out of its first stages | todo | never over a miracle bubble or worship icon |
| Over a leash post: "Leash" and the leash's kind (Aggression, Compassion, Learning) | todo | |

## Moving the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the land: "Move" | todo | |
| At the edges of the screen: "Rotate", "Pitch" or "Tilt/Rotate", by the hint the hand gives | todo | the hand's poses show it (see [navigation.md](navigation.md)), the words don't |
| "Zoom In", "Zoom Out", "Zoom" while zooming | todo | |
| "Zoom In (…)" naming the place a key zooms to | todo | |
| "Double-click" where double clicking would fly there | todo | |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a miracle, its name, and "Increase" or "Extreme" for its power-ups | todo | see [../miracles/](../miracles/) |
| Holding a miracle that needs a gesture: "Cast", "Cast Circle", "Gesture" | todo | see [../gesture/miracle_gestures.md](../gesture/miracle_gestures.md) |
| "Repeat Previous Miracle." | todo | |

## Elsewhere

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the temple, the tooltips of its rooms, doors, scrolls and creature | done | `src/3D/TempleToolTips.cpp`; tests `TempleToolTips.*`; see [../temple/](../temple/) |
| Over the player's creature, or holding it, the creature's status panel shows its needs and the hand's reward | done | `Game::UpdateHandInterface`, `src/Creature/CreatureStatusPanel.h`; test `test_creature_status_panel` |
