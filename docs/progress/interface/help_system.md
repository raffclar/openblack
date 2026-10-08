# Help system

The help the game gives as it is played: the advisors' messages when something happens for the first time or goes
wrong, the answers to the Help key, the creature's learning messages, alerts, banter and tips. Who the advisors are and
how they look and fly is in ../story/; the words on screen are in [on_screen_text.md](on_screen_text.md).

**Progress: 0/37 done, 2 partial — 3%**

## Running help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help messages come in sets, and a set picks one of its messages at random | todo | the texts are loaded (`src/Gui/TextDatabase.cpp`); nothing runs them |
| A message is spoken by the good or evil advisor, or both in turn, with their voice and lip sync | todo | see ../story/ and ../audio/ |
| Only one message runs at a time; a new, more important one stops the running one, the advisor coughing or grumbling at the interruption | todo | |
| A message waits until it has been read (by its length and the player's read speed) or clicked through before the next | todo | |
| Messages already given are remembered so they are not repeated | todo | |
| The help level setting sets how much help is offered, none turning it off | todo | ([options.md](options.md)) |
| Land scripts turn the help system on and off, and ask whether it is on | todo | `HELP_SYSTEM_ON` and `SET_HELP_SYSTEM` in `src/CHLApi.cpp` log "not implemented" |
| Help is held back while the player is inside the temple, and what was on screen comes back on leaving | todo | |
| Help can take the camera into widescreen bars while it speaks | partial | the bars exist (`CinematicDirectorSystem`, using the help system's widescreen time) but no help message drives them |
| What help has been given and how often is kept with the player's profile | todo | ([profiles.md](profiles.md)); its hand and camera counts are also shown on Tech Stats ([statistics_counted.md](statistics_counted.md)) |

## What triggers help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first time the player holds each kind of thing (each animal, each miracle seed, food, wood, rocks, a tree, a villager, a scaffold, poo, a ball, fire) | todo | |
| The first time each miracle is cast, and when it is powered up | todo | |
| The first time each miracle seed is seen | todo | |
| Wonders: on first seeing one, and for each tribe's wonder | todo | |
| Learning the controls: picking up by pulling, gestures on the ground, turning by the screen's sides, and the like, each praised once learnt | todo | |
| Looking at the sky, looking closely at the land, or bumping the camera against the land | todo | |
| Villagers who don't believe, who aren't interested, vagrants, children, and each kind of disciple | todo | |
| A challenge scroll lit up nearby | todo | see [scrolls_and_signs.md](scrolls_and_signs.md) |
| The village centre and the temple, the first times they are seen | todo | |
| Town alerts: the store running low on food or wood, few people, unhappy villagers, worshippers dying | todo | |
| Battle alerts: the player's people or towns attacked, the creature attacked or fighting | todo | |
| Remarks on the player's deeds: killing people, destroying buildings, being very good or very evil, acting against their alignment | todo | see ../worship/ for alignment |
| Remarks on the moon's phase and on the player not watching | todo | |
| Banter: idle chatter between the advisors from 25 sets | todo | |
| Reminders of what a challenge still needs | todo | see ../story/ |

## Asking for help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Help key asks about what is in the hand, else what the hand is over, else the ground under it | todo | F1 logs "not implemented" (`src/Input/ShortcutKeys.cpp`) |
| Each kind of thing has its own answer: villagers, animals and objects, buildings, the town centre, magic | todo | |
| The ground answers by its kind: solid ground, deep or shallow water, and so on | todo | |
| Help that teaches a control shows the key or mouse button to press, as the player has bound it | todo | ([key_bindings.md](key_bindings.md)) |

## The creature's help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tells the player what the creature wants now and why it failed at something | todo | see ../creature/ for the mind |
| Tells the player when the creature has learnt, or nearly learnt, an action or a spell, or can't learn one yet | todo | |
| Tells the player a desire or an opinion went up or down after a reward or a punishment | todo | |
| Walks the player through the stages of the creature's life (meeting the guide, the leashes, eating, fighting, helping and impressing towns, punishment, growing up) | todo | |
| Turned off by the creature help setting | todo | |

## Tips

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Did you know" texts, shown in the temple's library | partial | the library's scroll shows them (`src/3D/TempleScrolls.cpp`); see ../temple/ |
| "Did you know" scrolls lit in the world, read by tapping them | todo | ([scrolls_and_signs.md](scrolls_and_signs.md)) |
| Tips of the day | todo | (unconfirmed where the game shows them) |
