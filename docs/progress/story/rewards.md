# Rewards

Everything the game hands the player for finishing a challenge or a puzzle: the reward chest that falls from the sky
or simply appears, what is inside it (miracle seeds, miracle power-ups, food, wood, belief, toys, special objects), the
game's own list of which miracle to give next, the fireflies' reward chances in the land scripts, and the advisors'
lines about rewards. What each land's challenges give is in [land_1.md](land_1.md) to [land_5.md](land_5.md); how
challenges run and creature swaps are in [challenges_and_rewards.md](challenges_and_rewards.md); dispensers and
one-shot globes themselves are in [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md).

**Progress: 0/56 done, 4 partial — 4%**

## Giving a reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script gives a reward chest of one of 60 kinds at a place; any other kind is refused with a script error and no chest | todo | `CREATE_REWARD` is a stub in `src/CHLApi.cpp` (it pops its arguments and returns nothing) |
| A reward can also be given "in a town": the town then receives what is inside (its miracles, its belief, its scaffolds) instead of the player alone | todo | `CREATE_REWARD_IN_TOWN` is a stub in `src/CHLApi.cpp` |
| The chest always belongs to the local player, whoever's challenge gave it | todo | |
| The script chooses whether the chest falls from the sky or just appears on the ground | todo | the "from sky" flag is popped and ignored |
| A falling reward can come with a short film: the camera swings to one of two random views of the spot (from 80 m up and 60 m back, or from 15 m up and 15 m back, over 4 s), looks 150 m up the sky as the chest drops, follows it down over 4 s and then settles 25 m up and 30 m back, following the chest | todo | the cinematic camera exists (`CinematicDirectorSystem`), the reward films don't |
| A reward given outside a film does not wait for the player; the variant used before a fly-past waits for the chest to be opened before the script goes on | todo | |
| A miracle dispenser can be the reward instead of a chest: it is built where the script says, with its miracle, angle and refill time, and switched on | todo | dispensers exist (`MagicSystem::CreateDispenser`), but the script commands that place them are stubs; see ../miracles/dispensers_and_seeds.md; dispenser rewards: [The Singing Stones](silver_scrolls/the_singing_stones.md) (food), [The Hermit](silver_scrolls/the_hermit.md) and [The Explorers](silver_scrolls/the_explorers.md) (water), [The Saviour](silver_scrolls/the_saviour.md) (strength), [The Immersion Mushrooms](silver_scrolls/the_immersion_mushrooms.md) (compassion), [The Ogre](silver_scrolls/the_ogre.md) (heal), [The Pied Piper](silver_scrolls/the_pied_piper.md) (heal or lightning), [The Sea](silver_scrolls/the_sea.md) (big), [The Slavers](silver_scrolls/the_slavers.md) (wolf pack), [The Magic Dragon](silver_scrolls/the_magic_dragon.md) (flying flock) |
| The shared dispenser-reward script means to set the refill time only when one is given, but tests the game clock instead, so it always sets the refill time to the value passed, usually 0 | todo | what a refill time of 0 does to a dispenser is not determined; see [The Singing Stones](silver_scrolls/the_singing_stones.md) and [The Sea](silver_scrolls/the_sea.md) |
| A dispenser reward plays the reward sting and flies the camera to the dispenser (to a view 21.5 m to the side and 14 m up, over 4 s) | todo | the sting is listed in `src/Audio/Sound.h` but never played |
| Some challenges give a creature to swap for, an enabled miracle power-up for a town, or simply belief, instead of a chest | todo | see [challenges_and_rewards.md](challenges_and_rewards.md) and the land files; creatures: [creature_swaps.md](silver_scrolls/creature_swaps.md) (e.g. the tortoise of [The Fish Puzzle](silver_scrolls/the_fish_puzzle.md), the wolf of [The Treacherous Path](silver_scrolls/the_treacherous_path.md), the sheep of [The Lost Flock](silver_scrolls/the_lost_flock.md)); power-ups for a town: fireball from [The Idol](silver_scrolls/the_idol.md) and [Stanley The Wolf](silver_scrolls/stanley_the_wolf.md), lightning from [The Greedy Farmer](silver_scrolls/the_greedy_farmer.md) and [The Plague](silver_scrolls/the_plague.md); a healing temple from [The Beach Temple Puzzle](silver_scrolls/the_beach_temple_puzzle.md); a Wonder begun from [The Shaolin](silver_scrolls/the_shaolin.md) |
| Rewards are saved and loaded with the game (whether opened, the town they belong to, what they hold) | todo | no save system; see ../engine/saving_and_loading.md |

## The chest

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The chest is a closed wooden chest model; once opened it is swapped for an animated chest whose lid opens | todo | neither chest model is loaded by openblack (they live outside the mesh pack) |
| A cloud of 27 sparkles hangs round the chest, scattered within 2 m sideways and up to 2 m above it, each with a random frame, size and brightness | todo | |
| A falling chest drops straight down at a steady 50 m per unit of time from its starting height (the starting height and the time unit are unconfirmed) | todo | |
| When it reaches the ground it thumps down with its own landing sound and a small camera shake, and only then can it be opened | partial | Falls 150 m at 50 m/s turning π rad/s, thumps with sample 174, shakes the camera and raises its dust (`RewardSystem`, test `test_reward`); opening it isn't ported |
| The chest can't be picked up by the hand or thrown | todo | |
| Only the player it belongs to can open it, and only once | todo | |
| Hovering over it shows the chest's own tooltip rather than the usual one (unconfirmed wording) | todo | see ../interface/tooltips.md |
| Opening it is a click of the Action button on it: it plays a tap sound, then two opening sounds, and the lid opens | todo | see ../hand/ for clicking |
| An opened chest stays a short while and then is removed with its sparkles (the exact delay is unconfirmed) | todo | |
| The chest exploding into pieces has its own model in the game's mesh pack (when it is used is unconfirmed) | todo | the model is listed in `src/3D/AllMeshes.h` |

## What a chest can hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's reward table has 60 kinds, each naming the seed, gesture, power-up, help text, belief, object and scaffold it gives | partial | the table is read into `GRewardInfo` in `src/InfoConstants.h`; nothing uses it |
| Food in three sizes: opening adds a pile of food on the spot | todo | e.g. the large food reward of [The Lost Flock](silver_scrolls/the_lost_flock.md) |
| Wood in three sizes: opening adds a pile of wood on the spot | todo | |
| Belief in three sizes: with a town, the town gains that much belief for the player and a sparkle plays on the chest for 20 turns; without a town it gives nothing | todo | |
| A miracle seed (fireball, lightning, explosion, heal, teleport, food, storm, spiritual shield, physical shield, wood, skeleton, water, flying and ground flocks, and the creature spells: freeze, small, big, weak, strong, fat, thin, invisible, compassion, angry, hungry, frightened, tired, ill, thirsty, itchy) | todo | see "Miracle rewards" below; e.g. the heal chests of [The Sacrifice](silver_scrolls/the_sacrifice.md) and [Swap To Brown Bear](silver_scrolls/swap_to_brown_bear.md), the itchy seed of [The Heavenly Fire](silver_scrolls/the_heavenly_fire.md), the water reward of [The Hermit](silver_scrolls/the_hermit.md) |
| A miracle power-up (heal, fireball, lightning, explosion, storm, shield and skeleton levels) | todo | |
| Toys for the creature: a ball, a cuddly toy and a die | todo | the toy models are in `src/3D/AllMeshes.h`; see ../creature/object_actions.md; e.g. the toy ball of [Throwing Stones](silver_scrolls/throwing_stones.md) and the beach ball of [The Ogre](silver_scrolls/the_ogre.md); the toys themselves: [../nature/toys.md](../nature/toys.md) |
| Special objects: a singing stone, an idol or a weeping stone | todo | see ../nature/one_shot_features.md |
| A scaffold: with a town that has a workshop, a scaffold is made for it, already marked as a full one | todo | a single scaffold, counted as that workshop's: see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| The "next miracle" reward picks the miracle by the reward progress (below) | todo | |

## Miracle rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the player can't cast the miracle yet and the chest is in a town, the town learns the miracle for good and its icon appears there (at 0.8 size) | todo | worship-site icons are todo; see ../miracles/dispensers_and_seeds.md |
| If the player can't cast it and there is no town, a one-shot globe of that miracle appears 2 m above the chest | partial | one-shot globes exist (`MagicSystem::CreateOneOffSeedFor`); no chest makes them |
| If the player already has the miracle, the next power-up level the player lacks is given instead (to the town if there is one, otherwise to the player), with the miracle's gesture shown on the ground beside the chest | todo | |
| If the player already has every level of it, a one-shot globe of the miracle is given instead | todo | |
| A power-up reward with no seed just shows its gesture on the ground | todo | |

## Reward progress

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game keeps two ordered lists of 30 miracles each, one for good players and one for evil, and each entry says on which of the six lands it may be given | partial | read into `rewardProgressGood` / `rewardProgressEvil` in `src/InfoConstants.h`; nothing uses them |
| The "next miracle" reward first looks for a town of the player's to give it to, if none was given | todo | |
| It gives the next miracle the player's progress calls for; when there is none left it draws at random from the good list if the player is good, the evil list if evil, a coin toss if exactly neutral, repeating until it finds an entry allowed on the current land, and gives it as a one-shot globe | todo | which list is good and which evil is unconfirmed; see ../worship/alignment.md |
| What each of the 30 entries holds per land | todo | not yet read out of the game's data |

## Fireflies' reward chances

The land scripts list, for every miracle, a chance that a firefly gives it as a one-shot globe. The values are weights
read in order; how a firefly turns them into a reward is unconfirmed. Fireflies themselves are in
[../nature/one_shot_features.md](../nature/one_shot_features.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each land script lists a firefly reward chance for each of 42 miracles and power-ups | todo | the lines are parsed and dropped (`FireFlySpellRewardProb` in `src/LHScriptX/FeatureScriptCommands.cpp`) |
| Land 1: fireball, lightning, forest, food, wood and water at 1 each, heal at 20; everything else 0 | todo | |
| Land 2: fireball, lightning, heal, teleport, forest, food, storm, both shields, wood and water at 1; flying flock 0.1, ground flock 0.01; each creature spell (freeze, small, big, weak, strong, invisible, compassion, angry, itchy) 0.1 | todo | |
| Land 3: as land 2's plain miracles at 1, both flocks 0.1, every creature spell 0 | todo | |
| Land 4: the land 3 plain miracles at 1, plus fireball power-ups 0.2 and 0.1, lightning power-ups 0.2 and 0.1, heal and food power-ups 1, storm power-up 0.1, water power-up 0.2; flocks 0; each creature spell 0.1 | todo | |
| Land 5: plain miracles at 1, fireball power-ups 0.1 and 0.2, lightning power-ups 0.2 and 0.1, explosion 0.1, heal power-up 0.2, food power-up 1, storm power-ups 0.1 and 0.1, both flocks 0.5, each creature spell 0.2 | todo | |
| The tutorial land gives fireflies no rewards (every chance 0) | todo | |
| Fireflies come out at night and hide by day, so firefly rewards come only at night | todo | see ../nature/one_shot_features.md |

## Help text about rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game counts the rewards given; the very first one gets its own lines: an advisor wonders what is in the chest, the other says to click the Action button on it | todo | the dialogue commands are stubs in `src/CHLApi.cpp`; see [advisors.md](advisors.md) |
| When the first chest is opened, the reward's own help line is spoken, then "now click on the contents to activate them" | todo | `GET_HELP` is a stub in `src/CHLApi.cpp` |
| Every later chest, once clicked, has its reward's help line spoken in one line if the dialogue is free | todo | |
| Each kind of reward has its help line (food, wood, forest, storm and its gestures, teleport, both shields, heal, heal many, fireball and its gestures, a "question" line, and forty more numbered ones) | todo | |
| The first dispenser reward: an advisor explains miracle dispensers, a "did you know" scroll about miracles is placed, a signpost is put up beside the dispenser and the advisor points at it | todo | see ../interface/scrolls_and_signs.md |
| Later dispenser rewards get a one-line "another dispenser" remark, then the dispenser's first-to-last help lines | todo | `GET_FIRST_HELP` and `GET_LAST_HELP` are stubs |

## Rewards by land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What each land's gold and silver scrolls give | todo | see [land_1.md](land_1.md), [land_2.md](land_2.md), [land_3.md](land_3.md), [land_4.md](land_4.md), [land_5.md](land_5.md); every silver scroll and its reward: [silver_scrolls.md](silver_scrolls.md) |
| What the puzzles and games give (dispensers, seeds, toys, creatures, power-ups) | todo | see [minigames.md](minigames.md) |
