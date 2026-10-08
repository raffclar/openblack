# Testbed scenarios

The ready-made scenarios of the testbed (about 200 of them), grouped by what they show, and the parts of the game that
have none yet. A row is done when there are scenarios that show that part working; the framework is in
[testbed.md](testbed.md).

**Progress: 25/34 done, 4 partial — 79%**

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Idle: fidgeting, hanging around, wandering | done | `src/Debug/TestbedScenarioRegistry.cpp` (Idle) |
| Faces, emotions, gestures and the emotes of its desires; eyes and blinking | done | (Expressions) |
| What it looks at: watching a walker | done | (Senses) |
| Needs: thirst, hunger, sleep by night and day, poo, puking, fainting, cold, exhaustion | done | (Needs, 14 scenarios) |
| Growing up: a time-lapse, the stages unlocking desires, the morph lineup | done | (Growth) |
| Looks: skins and hair by alignment, tattoos and wounds | done | (Appearance) |
| Footprints per species, in snow, grass and shallows, the first of April smileys | done | (Footprints) |
| Moving: walking, running and turning, routes round obstacles, round the lake and through small trees | done | (Movement) |
| Objects: picking up, looking over, putting down, throwing, eating, knocking down trees, pointing | done | (Objects) |
| The hand on it: stroking each part, slapping, the status panel, the hand's look by alignment | done | (Hand, 17 scenarios) |
| The leashes: leading, tying, keeping home, the leash keys and picker, other players' creatures | done | (Leash, 15 scenarios) |
| Fights: by themselves, charged and quick blows, blocks, knock-outs, slapping other gods' creatures | done | (Combat) |
| Learning: rewards and punishments, by watching, from mind files, copying the player | done | (Mind) |
| Creature Mode, following it with C, and the Creature Cave with its tattoos | done | (CreatureMode) |
| Its voices by species and size, and the sounds of the land | done | (Audio) |
| Its shadow, its reflection, light on the land through the day | done | (Light) |

## Miracles and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every miracle cast, held, powered up and its effect, each in its own scenarios | done | (Miracles, about 87 scenarios), `TestbedScenarioMiracles.cpp` and the per-miracle files |
| Dispensers, globes and seeds from worship | done | `TestbedScenarioGlobes.cpp` |
| The creature casting its spells | done | `TestbedScenarioCreatureCasting.cpp` |
| Fire spreading, blasts, water putting fire out | done | `TestbedScenarioBlastFire.cpp`, `TestbedScenarioFirewater.cpp` |
| Particles and sprites: sparkles, smoke, steam, mist, sorted sprites | done | (Particles) |
| Gestures drawn through the recogniser | done | `TestbedScenarioGestures.cpp` |
| Hand navigation: dragging, edge turning, tilting, both buttons, middle button | done | `TestbedScenarioHandNavigation.cpp` |

## Other uses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set ups for trying the editor on | done | (Editor) |
| Crowd benchmarks of creatures and villagers | done | (Benchmark), `TestbedCrowd.cpp` |

## Parts of the game without scenarios

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers' lives and jobs | todo | villagers appear only as onlookers and targets |
| Towns growing, their desires and building | todo | |
| Buildings: construction, the workshop's scaffolds, damage and repair | todo | |
| Wild animals and livestock on their own | partial | herds and flocks appear in the miracle scenarios only |
| Worship and prayer power from worshippers | partial | prayer power is set by the scenario; worship itself has none |
| Weather on its own: climates, rain and snow, wind | partial | storms appear with the storm miracle; the debug Weather window forces the rest |
| The temple and its rooms | todo | |
| Land scripts and challenges | todo | |
| Physics of thrown objects other than the creature's and the miracles' | partial | throwing at targets with the creature and fireballs by hand only |
