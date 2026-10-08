# Puzzles and games

The puzzle objects and small games the lands' challenges are built on: the tree puzzles, the beach temple rings, the
Theseus maze, the lion maze, the fish herding, the shaking mushrooms, throwing stones, the singing stones, the man who
wants to be thrown, the shield stones, and the villagers' football with its ball. Each row is a rule of the game
itself; the story around each challenge (who asks, the films, the alignment) is in the land files,
[land_1.md](land_1.md) to [land_5.md](land_5.md), and what the rewards do is in [rewards.md](rewards.md). The fourth land's
bell memory game belongs to its gold scroll, [The Totem Puzzle](gold_scrolls/the_totem_puzzle.md).

Only the games compiled into Black & White's own challenge file count. The engine and the script sources also hold
puzzle kinds and games no Black & White land uses; they are listed at the end as n/a.

**Progress: 0/89 done, 1 partial — 1%**

## Puzzle objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script places a puzzle of a given kind at a place, with an angle and a size | todo | `CREATE` and `CREATE_WITH_ANGLE_AND_SCALE` only make scenery and rocks (`CreateScriptObject` in `src/CHLApi.cpp`); the puzzle kinds are listed in `src/ScriptHeaders/ScriptEnums.h` |
| A script can ask a puzzle whether it is not yet begun, in progress, won, lost, won the good way or won the evil way | todo | `STATE` queries on objects are not implemented |
| A puzzle checks every game turn whether it is solved; once solved it stays solved and the script's "played" test becomes true | todo | |
| A puzzle's pieces (trees, rings, animals, markers) are the puzzle's own: if one is destroyed it is put back | todo | |
| Puzzles are saved and loaded with the game, pieces and all | todo | no save system |
| Each puzzle has a "did you know" signpost beside it with its rules, removed when the puzzle is solved | todo | see ../interface/scrolls_and_signs.md |

## Tree puzzles (lands 2 and 3)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine trees stand on a three-by-three grid; each spot has a state that shows as the kind of tree growing there | todo | |
| Pulling a tree off its spot changes the state of that spot and of the four spots beside it (not the corners); each changed spot gets a new tree of its new kind, which starts small | todo | |
| The land 2 puzzle has two states per spot; the land 3 puzzle has three | todo | |
| New trees grow back by a twentieth of their full size each step until full (the step is unconfirmed) | todo | see ../nature/trees.md |
| The puzzle is solved when all nine trees are fully grown and all nine spots are in the same state; the trees then stay as ordinary trees | todo | |
| Land 2's prize is a flying-flock miracle dispenser that refills every 7 minutes; land 3's (which appears only after the player owns the nearby Indian town) is a flying-flock seed falling from the sky into that town, with its street lamps faded away | todo | see [land_2.md](land_2.md), [land_3.md](land_3.md) |

## Beach temple rings (land 2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A beach temple made of four rings of different sizes stands on one of three bases | todo | |
| Rings are moved by hand; a dropped ring settles onto the stack under it if one is within 10 m | todo | |
| A ring can rest on a base or on a larger ring, never on a smaller one | todo | |
| A ring stacked on another sits higher by the height of the ring below (4, 3, 2 or 1 m from the largest down) | todo | |
| A base moved off its place is put back | todo | |
| Solved when all four rings are stacked largest to smallest on the base up the beach, away from the tide | todo | |
| The prize: the temple heals every living thing within 10 m of it, renewed every 20 s, for good | todo | see [land_2.md](land_2.md); the quest around the puzzle: [silver_scrolls/the_beach_temple_puzzle.md](silver_scrolls/the_beach_temple_puzzle.md) |

## Theseus maze (land 4)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A walled grid holds a hero and a monster; the player clicks the ground of a square next to the hero, or the hero's own square to wait, and a marker shows the clicked square | todo | |
| The hero steps one square that way at 4 m/s unless a wall is in the way | todo | |
| Then the monster takes up to two steps towards the hero, each time trying across first and then along, and only where no wall blocks it | todo | |
| If the monster reaches the hero's square, or either of them is killed, the maze starts again | todo | |
| Solved when the hero walks out of the maze's exit | todo | |
| Land 4 has two mazes, the second appearing when the first is solved; the prize is a "strong" creature-spell seed falling from the sky into the player's home town | todo | see [land_4.md](land_4.md) |

## Lion maze (land 5)

The engine calls this puzzle the lion maze, but on land 5 the animal in it is a wolf (Stanley) and its target a sheep;
the quest around it is [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wolf stands in a walled grid; four bells round it each send the wolf one way, and a fifth, at a corner, starts the maze again | todo | the engine's moving piece is a wolf and its target a sheep, see [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |
| Tapping a direction makes the wolf turn and then slide that way, square after square, until a wall stops it, walking at 4 m/s; each tap flashes the bell and plays its sound | todo | |
| If the wolf or the sheep is killed or lost, the maze starts again | todo | |
| Solved when the wolf stops on the sheep's square; both vanish in a sparkle | todo | |
| The prize: a lion creature to swap for, and the fireball's second and third levels for the player's home town; the script only gives it while the wolf's owner lives, but he is made indestructible and can never die, so the prize always comes | todo | see [land_5.md](land_5.md) and [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |

## Fish herding (land 4)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two shoals of 15 fish each are made in the sea, 11 m across, swimming at 7 (the unit is unconfirmed) | todo | see ../animal/ |
| The fish swim away from the hand, so the player herds them by tapping the water | todo | |
| Solved when the shoal reaches where the boy fishes (the exact test is unconfirmed) | todo | |
| The prize, while the boy lives: a tortoise creature to swap for; five tortoises wander with him | todo | see [land_4.md](land_4.md); the quest: [silver_scrolls/the_fish_puzzle.md](silver_scrolls/the_fish_puzzle.md) |

## Shaking mushrooms (land 1)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge only exists when a force-feedback mouse is plugged in; otherwise nothing happens and the land's mushrooms stay as they are. With the mouse, the mushrooms already standing near the puzzle are removed first | todo | openblack has no force-feedback support; the quest: [silver_scrolls/the_immersion_mushrooms.md](silver_scrolls/the_immersion_mushrooms.md) |
| 18 mushrooms are made by a hut, scattered in a square round a cauldron in the same layout every game; each shakes in the hand through the mouse, and the ninth shakes most | todo | |
| Dropping the ninth mushroom in the cauldron wins, any other loses | todo | |
| Won: a level 2 heal is cast on the hut and the prize is a compassion creature-spell dispenser | todo | |
| Lost: the hut catches fire and an explosion is cast on it (50 m across, for 30 s); the blast throws the man through the air to land where the camera is | todo | see [land_1.md](land_1.md) |

## Throwing stones (land 1)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A boulder sits on a pillar as the target; knocking it off its place (more than 2 m) wins | todo | the quest: [silver_scrolls/throwing_stones.md](silver_scrolls/throwing_stones.md) |
| A pile of five rocks (half to nine tenths of full size) refills while the quest runs: a rock that is destroyed comes back, and one left more than 40 m from the pile is put back at half size; once the quest is won the pile stops refilling | todo | |
| A villager watches each throw: if the rock passes within 12 m of the pillar he cheers, otherwise he despairs | todo | see ../villager/ |
| He ducks when a rock in the hand comes within 10 m of him | todo | |
| If his hut falls below three quarters of its health he storms off to a friend's house | todo | |
| Hitting the pillar while the boulder stays on it earns half marks and a word of praise | todo | |
| The prize is a toy ball falling from the sky by the target | todo | see [rewards.md](rewards.md) |
| Afterwards a boulder comes back on the pillar; each time it is knocked off it turns into a water seed, six times in all | todo | |

## Singing stones (lands 1 and 2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1's circle has eight singing stones, each with its own note, and three wrong stones with a sour note; tapping a stone plays its note | todo | the singing stone music is in `src/Audio/GameMusic.cpp`; nothing plays it; the quest: [silver_scrolls/the_singing_stones.md](silver_scrolls/the_singing_stones.md) |
| The circle is complete when the five loose stones are back in their own holes; they are then fixed in place and the circle plays its tune | todo | |
| Putting wrong stones in the holes upsets the hippy who lives by the circle | todo | |
| The circle's prize is a food miracle dispenser | todo | see [land_1.md](land_1.md) |
| Land 2's stones remember the last 14 taps and listen for three tunes | todo | the quest: [silver_scrolls/the_singing_stones_land_2.md](silver_scrolls/the_singing_stones_land_2.md) |
| The first tune makes night fall, with a flock of 10 bats and a mist | todo | |
| Another tune raises the dead within 10 m of the circle for 5 minutes: dead villagers come back as skeletons of the same age, joining the town with id 11 (the Indian town by Lethys's snow edge) whichever town they came from, and dead animals come back as they were | todo | the script calls it the nearest town, but it is always that one |
| Each tune played adds a third to the challenge's success | todo | see [land_2.md](land_2.md); the stones themselves: ../nature/one_shot_features.md |

## The man who wants to be thrown (land 3)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A man by a campfire can't be hurt or burnt and keeps asking to be picked up and thrown | todo | |
| Throwing him slows the game to a third of its speed (0.32) for 3 s, the camera following him from 10 m, then the speed eases back to normal over 3 s | todo | see ../engine/game_loop_and_clock.md |
| After landing he walks back to his place and boasts it didn't hurt | todo | |
| Landing in the sea, he is made again at his place once out of view | todo | |
| If the creature eats him it is made to poo, and he comes out of it again | todo | see ../creature/physiology.md |
| Set alight, he begs for the fire to be put out | todo | |

## Shield stones (land 5)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three stones, each with a chanting villager, beam power to a point 80 m above a town and a vertical beam holds a shield over it | todo | see ../miracles/spiritual_shield.md |
| The shield's radius is 35 m for each stone still beaming | todo | |
| A stone beams while it is within 1 m of its place and its chanter lives at his spot; moving or breaking it stops it | todo | |
| A rock, tree or fireball flying within 40 m of a chanter startles him away for 45 s | todo | |
| A beaming stone strikes the player's creature with lightning when it is within 100 m, unless it is invisible | todo | |
| It starts when the town has any belief in the player or the creature comes within 300 m | todo | see [land_5.md](land_5.md) |

## Land 4's totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two totems start lowered; raising both to full height frees the skeleton village | todo | see [land_4.md](land_4.md) and ../town/artefacts.md; every step: [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md#raising-the-totems) |
| A bronze "Did you know" totem puzzle north-west of the Japanese village uses the engine's third totem layout: six totems each turning through five positions, where changing one moves up to five others, until all are fully raised; a ring of influence (radius 30) surrounds it, and solving it gives a shield miracle dispenser | todo | puzzle objects aren't made by scripts in openblack; see [land_4.md](land_4.md) and [gold_scrolls/the_totem_puzzle.md](gold_scrolls/the_totem_puzzle.md#quirks-unused-and-cut-parts) |

## Football

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town's football pitch is a building with goals, corner flags and a centre circle, in two sizes, and comes with a ball | partial | the pitch is placed (`src/ECS/Archetypes/AbodeArchetype.cpp`); its ball and parts are not; see ../building/civic_buildings.md |
| Ten places: per side two attackers, two defenders and a goalkeeper | todo | see ../villager/play_and_gossip.md |
| A match waits for at least three players on each side, then for the ball to be put on the centre spot and most players to be in place, and kicks off | todo | |
| Players who wait too long without enough players are sent away | todo | |
| A match goes back to waiting when a side has no players, or both sides have only one | todo | |
| A match stops when the town no longer most wants playtime or relaxation, once it has run 1800 turns | todo | see ../town/ |
| The score is kept, home against away | todo | |
| After a goal play stops for 70 turns, then restarts from the centre | todo | |
| When the ball goes dead the nearest player fetches it for the restart while the others take their places | todo | |
| The ball is removed when it goes more than 100 m from the pitch or into water, or is carried too far away in a hand (a new one is then made, unconfirmed) | todo | |
| Attackers shoot at goal at 12 to 16 m/s, lob near goal at 3 to 4 m/s aiming within 2 m of it, and dribble with taps of 6 to 7 m/s a fifth of the way on | todo | |
| Defenders clear and save at 8 to 11 m/s aiming within 5 m, and mark players | todo | |
| The goalkeeper saves with kicks of 9 to 10 m/s and goes for a loose ball one time in five; outfield players go two times in three | todo | |
| Each player picks what to do by weighted chance (pass 0.8 to 1, save 0.7 to 1, clear 0.6 to 1, mark 0 to 1) | todo | |
| Spectators do a Mexican wave, one more joining each turn | todo | |
| The player or creature can take the ball and throw it; the creature learns from scoring and catching | todo | see ../creature/town_actions.md and ../creature/learning_by_observation.md |
| A football match impresses those watching (it has its own impressiveness kind) | todo | see ../worship/ |

## The ball

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ball is a toy the creature can pick up, kick and throw | todo | a ball object can be made only from the debug creature spawner (`src/Debug/CreatureSpawnerHands.cpp`); see ../creature/object_actions.md and [../nature/toys.md](../nature/toys.md) |
| A ball has its own bouncy physics | todo | see ../physics/object_dynamics.md |
| Creatures and villagers react to a ball near them and go to play | todo | see ../creature/reactions.md |

## Not used by Black & White's lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two maze puzzles | n/a | an engine puzzle kind no land uses |
| Three of the four totem puzzle layouts: six totems each turning through five positions, where turning one moves the others by set amounts | n/a | no land uses them; the third layout is Land 4's Japanese bronze puzzle (above) |
| A second lion maze, a second fish puzzle and a second mushroom puzzle | n/a | engine puzzle kinds no land uses |
| Lions and sheep: a take-away game against the computer, which plays the winning move when there is one | n/a | an engine puzzle kind no land uses |
| Chess | n/a | the kind exists but has no rules in the game |
| Cow bowling, bowling, whack-a-villager, catching villagers, a race, Simon says, the cup final, a spitting totem, the big whale, raiding lions | n/a | script sources not compiled into the game's challenges; the cup final: [../town/football.md](../town/football.md) |
| Creature Isle's own games | n/a | see [creature_isle.md](creature_isle.md) |
