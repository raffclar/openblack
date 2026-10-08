# Creature tools

The creature spawner: openblack's window for making creatures, looking inside them and telling them what to do. It
stands on its own and is also hosted in the editor's inspector for a picked creature.

**Progress: 29/29 done, 0 partial — 100%**

## Spawning and picking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spawn a creature of any species with its body set up (size, alignment, fatness, strength, age), facing a way or at random | done | `src/Debug/CreatureSpawner.cpp` "Spawn" tab |
| Put the body back to the species' defaults | done | "Species defaults" |
| List the creatures on the land, pick one, remove one or all | done | "Creatures" tab |
| The picked creature's tab, which the editor's inspector shows too | done | "Selected" tab; `src/Editor/Panels/InspectorPanel.cpp` |

## Movement and actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Order it to walk or run to a point, flee from it or face it | done | "Movement" |
| Order it to pick up, throw at, knock down or point at a thing | done | |
| Stop it, turn it to the camera, and show its route | done | |
| Pause its mind so the body is posed by hand | done | "Pause mind" |
| Play any animation, gesture, sit, stand | done | "Tell it" |
| Stroke or slap it as the hand would | done | |
| Show its hair, and its footprints, with the first of April smileys | done | `CreatureSpawnerFootprints.cpp` |

## Body and needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fill up, starve, tire out, parch or bloat it | done | `CreatureSpawnerBody.cpp` |
| Have it sleep, wake, eat, drink, poo, puke or faint, or drop food in front of it | done | |
| Turn fainting from exhaustion on and off | done | "Creatures faint" |

## Looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tattoos: pick a slot and colour, put it on or clear it | done | `CreatureSpawnerAppearance.cpp` |
| Wounds, burns and blood anywhere on its body, healed a step at a time, with the heal effect | done | |
| Drawing switches: blending its seams and its shadows | done | |

## Hands, leash and fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hold things in either hand; put down, toss, lob, eat, drop or look over what it holds | done | `CreatureSpawnerHands.cpp` |
| Make it leashable, put the leash on or off, tie it to a thing or another creature, keep it home or let it roam | done | `CreatureSpawnerLeash.cpp` |
| Place leash posts at the hand and show the rope's points | done | |
| Start a fight between two creatures, or let them fight by themselves; knock out, kill or bring round | done | `CreatureSpawnerFight.cpp` |
| Block and special moves on command, and the camera watching the fight | done | |

## Mind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Open a mind file through a file dialog, start a fresh mind, save the mind back | done | `CreatureSpawnerMind.cpp`, `CreatureSpawnerMindFiles.cpp`, `src/Debug/FileBrowser.cpp`; test `test_file_dialog.cpp` |
| Load one of the game's own mind files | done | "game" list |
| See and change its desires and likes, opinions of actions, decision trees and attitudes | done | |
| Teach it by rewarding or punishing, clear what it has learnt | done | |
| Show it a skill or a miracle to copy, cast or do it near | done | "Skills, miracles and copying" |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Mute creatures, or other players' creatures' voices | done | `CreatureSpawnerAudio.cpp` |
| Play each animation's sounds | done | |
