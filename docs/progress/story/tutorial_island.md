# Tutorial island (the Gods' Playground)

A separate small island, the Gods' Playground, where the Island Keeper walks the player through the controls in six
lessons: dragging and turning, tilting, all of them together, zooming, picking things up and double clicking. It is not
part of the story: the player goes there from the first land with F2, which the advisors suggest, and comes back with
Escape. The story's own teaching, in the opening of the first land, is in tutorial.md.

The game over cannot happen here, as the player has no temple: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/53 done, 5 partial — 5%**

## Going there and back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors tell the player about the Gods' Playground and that F2 takes them there | todo | advisor lines; see advisors.md |
| F2 asks "Are you sure you want to go to the Gods' Playground?" | todo | openblack has no F2 binding for it |
| F2 does nothing in a multiplayer game, while a script holds the cinema bars, or when already there | todo |  |
| Yes: the game is quick-saved to a reserved slot, every script stops and the playground island loads | todo | see ../engine/saving_and_loading.md |
| The island's own script starts and the interface is put back to normal | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| No closes the question and play goes on | todo |  |
| Escape stops the lesson running; Escape again asks "Are you sure you want to leave the Gods' Playground?" | todo |  |
| Leaving reloads the quick-saved game, putting the player back where they were ("back to Eden") | todo |  |
| The island can be loaded on its own | partial | it shows in the debug menu's lands (`Game.cpp` loads every land script), without its lessons |

## The island

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A small land of its own, numbered 6, with a mountain, a valley path, a ditch and an offshore islet | partial | the land loads (`Data/Landscape/LandT.lnd`); see ../scripts/landT_script.md |
| Two of the player's towns (Japanese and Celtic) and a neutral Indian one, with houses, fish farms and fields | partial | loaded by the land script; see ../town/ |
| Hundreds of animals, trees, big forests, fireflies and lanterns | partial | see ../animal/, ../nature/ |
| The player's influence covers the island (a ring of radius 1000 at its middle) | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| The Island Keeper, a monk who can't be picked up, moved or hurt | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| A field of miracle dispensers, one for every miracle: the 25 player miracles in four rows and the 16 creature spells in two | todo | see ../miracles/dispensers_and_seeds.md |

## Running the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lessons run in order, each starting when the last is done | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Space restarts the lesson running | todo |  |
| Each lesson limits the interface to what it teaches | partial | the interface level command works (`src/CHLApi.cpp`); see ../hand/ |
| Each lesson's lines are said by the Island Keeper, and wait to be read | todo | see advisors.md |
| A lesson that the player strays from has the Keeper call them back ("You are straying! Try and keep me in view!") | todo |  |
| Each line names the mouse button or key to press, as the player has bound it | todo | see ../interface/key_bindings.md |

## Lesson 1: moving and turning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper welcomes the player and sets off round the mountain, beckoning | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Only grabbing and dragging the land works | todo |  |
| The player keeps up by grabbing the land and pulling it | todo | the camera control itself: see ../camera/world_camera_controls.md |
| The Keeper waits while he is out of view or more than 100 away | todo |  |
| Then turning: the hand at the screen's left edge shows the rotation arrows; holding the button and moving the mouse turns the view | todo | the camera control itself: see ../camera/world_camera_controls.md |
| Only turning and dragging work until the player has turned the view the way asked | todo | the camera's rotation check; see ../camera/ |
| The evil advisor reminds the player how to turn if they forget | todo |  |
| A timed round: follow the Keeper round the mountain in under five minutes; he stops if the player is more than 50 away or ahead of him | todo |  |
| Too slow: "Oh. Bad luck" and the round starts again | todo |  |

## Lesson 2: tilting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper climbs a path and the player follows by tilting the view | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Only tilting works | todo |  |
| The hand at the screen's top edge shows the arrows; holding the button and moving the mouse tilts the view | todo | the camera control itself: see ../camera/world_camera_controls.md |
| Then the Keeper walks down again and the player keeps him in view | todo |  |
| Too slow or straying: the step starts again | todo |  |

## Lesson 3: everything together

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper walks round a valley path and the player follows with every control | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| He waits when out of view or too far | todo |  |

## Lesson 4: zooming

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding both mouse buttons and moving the mouse zooms | todo | the camera control itself: see ../camera/world_camera_controls.md |
| The player zooms out to see the whole island, then back in | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Two rocks are placed as marks to zoom between | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| The keyboard and the mouse wheel zoom too | todo | the camera control itself: see ../camera/world_camera_controls.md |

## Lesson 5: picking up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five lost teddy bears to pick up and drop in a ditch | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Picking up: the hand over a bear, the Action button held and the mouse moved | todo | see ../hand/picking_up.md |
| Dropping: the Action button again | todo | see ../hand/holding.md |
| All five in the ditch ends the lesson | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |

## Lesson 6: double clicking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Double clicking the Move button somewhere flies the camera there | todo | the camera control itself: see ../camera/world_camera_controls.md |
| The player double clicks the islet offshore to fly there | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Getting there any other way: "Hey! You didn't double-click to get there!" and try again | todo |  |
| Then double clicks the mountain top to fly back | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |

## After the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper says that was all and the player may stay and practise | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| Six gold scrolls stand by the start, one per lesson; tapping one runs that lesson again | todo | see ../interface/scrolls_and_signs.md |
| The Keeper goes; each replay brings its own Keeper | todo | the script commands it needs are stubs; see ../scripts/landT_script.md and ../scripts/ |
| The miracle dispensers stay for practising miracles | todo | see ../miracles/dispensers_and_seeds.md |
