# Villager looks, voices and special villagers

How villagers look and sound: their tribe's models, their animations, what they carry, their names, the special
characters that wander the lands, and the voices of villagers talking and praising the player.

**Progress: 2/20 done, 3 partial — 18%**

## Models and drawing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each villager draws its tribe's model for its job, high or low detail by distance | partial | `src/ECS/Archetypes/VillagerArchetype.cpp` always uses the high-detail model |
| Women, men and children each have their own models | partial | job decides the model; no child models |
| Villagers animate: walking synced to the ground covered, working, eating, dancing, idling, dying | todo | villagers play no animations (noted in `VillagerShieldShelter.cpp`) |
| Clips play into and out of states, and the state waits for them | todo | |
| What a villager carries shows in its hand (tools, logs by tree type, food, the ball) | todo | See tools_and_carried_items.md |
| Villagers are drawn smoothly between game turns and lean with slopes | todo | (unconfirmed whether openblack interpolates villagers; no code found) |
| Each foot casts a short ground shadow | done | `src/Graphics/GroundBlobs.h`, `src/Graphics/Renderer.cpp`; see ../rendering/ |
| Poisoned villagers show they are poisoned | todo | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Disciple icon over a disciple's head | todo | See disciples.md |
| Villagers at home are hidden | done | `src/ECS/Components/AtHome.h` |

## Names and information

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only named villagers have a name (about 2 in 10 new villagers; see [special_villagers.md](special_villagers.md)), shown under the hand | todo | the key bindings exist (`src/Gui/GameMenu.cpp`, show villager name and details) but nothing is drawn |
| A villager's details show on request | todo | the editor's inspector shows the component (`src/Editor/Panels/InspectorPanel.cpp`), not the game's display |
| Tooltip for what a villager is doing | todo | See ../interface/ |

## Special villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Now and then a created villager is a special one with its own look and name (hermit, sculptor, piper, sailor, priest, healer and the rest) | todo | the roles are listed in `src/Enums.h` only |
| Special villagers speak with a speech bubble | todo | |
| Story characters are villagers driven by the challenge scripts | todo | See ../story/ |

## Voices and speech

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Awed villagers praise or fear the player aloud, good or evil, more awed as their town nears being won | partial | timed and chosen in `src/ECS/Systems/Implementations/VillagerReactions.cpp`; see ../audio/ for whether the samples play |
| Villagers talk to each other in their tribe's babble | todo | |
| Villagers shout and scream when fleeing, burning or dying | todo | See ../audio/ |
| Villagers call out tribe-specific lines when the town needs something | todo | |
