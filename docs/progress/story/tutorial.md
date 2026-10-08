# Tutorial

How the game teaches itself: the opening of the first land, where the advisors and short hand demonstrations show the
camera, the hand and the first miracles. The separate practice island is in tutorial_island.md.

A cut early draft of the opening at the temple (rotating the camera, carrying wood and food to the builders, the kinds
of scroll) is in [silver_scrolls/see_the_citadel.md](silver_scrolls/see_the_citadel.md).

**Progress: 0/17 done, 3 partial — 9%**

## The opening scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Norse family (father, mother and son) is made, in high detail, at early morning | todo | the high-detail villager models and eyes are ported on Diego's bw-clean branch, not in openblack |
| The scene takes the camera, the dialogue and the game speed, brings the cinema bars in and starts its music | partial | the bars and music work; taking the camera, dialogue and speed are stubs |
| The parents kiss, the son swims out and sharks come for him | todo | see land_1.md |
| The screen fades to black and the intro film plays | todo | see ../video/bink_videos.md |
| A light falls from the sky, a ghostly hand lifts the boy from the sea and sets him down on the beach | todo | the intro's light and hand are on Diego's branch only |
| The village crowds round to welcome their new god; the advisors appear and introduce themselves | todo | see advisors.md |
| A demonstration of dragging the land, then the scene hands control to the player | todo | the hand demo command is a stub |
| About four minutes from the start to the hand-over | todo | (per Diego's notes) |

## The opening of the first land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player follows the family to the village, learning to move over the land | todo | see land_1.md; the family's script also starts the gate-stone guards: [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md#the-stones-are-guarded) |
| Hand demonstrations: a ghost hand shows a movement (rotating, tilting, dragging the land, zooming, throwing, giving food) | todo | the hand demo command is a stub; the demo recordings in `Data/HandDemo` are not read |
| A demonstration waits for the player to copy it, and reminds them if they don't | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Lessons on worship, influence and casting a miracle by gesture | todo | see land_2.md (Khazar's lessons) |
| Each lesson's key or mouse button is shown as the player has bound it | todo | see ../interface/help_system.md |
| The camera's features are allowed one by one as each is taught | partial | the camera help system keeps what the scripts allow (`src/ECS/Systems/CameraHelpSystemInterface.h`); the commands that allow them are stubs |
| The interface is limited during the lessons (only grabbing, only rotating …) | partial | the interface level command works (`src/CHLApi.cpp`); what each level allows is see ../interface/ |
| An Immersion force-feedback mouse is detected and welcomed | n/a | hardware long gone |
| A returning player can skip the tutorial and the creature's training (patch 1.1) | todo | see land_1.md |

## The tutorial island

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A separate practice island, the Gods' Playground, reached with F2 | todo | see tutorial_island.md |
