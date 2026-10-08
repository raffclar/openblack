# Silver scrolls

The story's own quests, the gold scrolls, are listed in [gold_scrolls.md](gold_scrolls.md).

Every silver scroll of the five story lands, land by land, with links to each quest's own file in
[silver_scrolls/](silver_scrolls/). This is an index, not a feature file: it has no rows of its own and is not scored;
the numbers below are summed from the quest files.

Gold scrolls are the story: the player has to finish them to move on, and they are covered in the land files
([land_1.md](land_1.md) to [land_5.md](land_5.md)). Silver scrolls are the optional challenges: a silver scroll stands
over whoever asks for help, tapping it starts the quest, and the quest is kept in the challenge log with how well it went
and how good or evil the player was. How challenges and their rewards work in general is in
[challenges_and_rewards.md](challenges_and_rewards.md) and [rewards.md](rewards.md).

The list was drawn up from the game's challenge scripts: the 47 script files that put up a challenge highlight were
each checked against every land's control script to see whether, when and on which land the game starts them, and the
set that ships was taken from the compiled challenge program, so a quest that is only in the script sources is listed as
cut. Each table is in the order the land offers its quests, as far as the control script fixes it.

## Land 1

The land's control script starts the throwing lesson and the lost flock straight after the opening, the explorers and
the breeder once the creature is chosen, then the rest one by one as the creature's lessons go on.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [Throwing Stones](silver_scrolls/throwing_stones.md) | the advisors, over the hill above the pillar | a toy ball, then up to six water miracle seeds for practice throws | 50 | 5% |
| [The Lost Flock](silver_scrolls/the_lost_flock.md) | a Norse shepherd by the player's village | a large food reward at five sheep; a sheep creature to swap to at ten | 83 | 2% |
| [The Explorers](silver_scrolls/the_explorers.md) | three sailors by an unfinished ark on a beach | a water miracle dispenser | 93 | 4% |
| [The Immersion Mushrooms](silver_scrolls/the_immersion_mushrooms.md) | a hippy at his hut (only with a force-feedback mouse) | a compassion creature miracle dispenser, for the right mushroom | 42 | 7% |
| [The Singing Stones](silver_scrolls/the_singing_stones.md) | a hippy by the stone circle | a food miracle dispenser | 71 | 7% |
| [The Hermit](silver_scrolls/the_hermit.md) | a hermit at his hillside hut | a water miracle dispenser, or a water miracle from the sky if his hut was damaged | 91 | 6% |
| [The Saviour](silver_scrolls/the_saviour.md) | a fisherman's wife on the shore | a strength creature miracle dispenser, for saving all five or killing all five | 53 | 3% |
| [The Pied Piper](silver_scrolls/the_pied_piper.md) | the crèche woman of the Norse village | a heal miracle dispenser (good ending) or a lightning miracle dispenser (evil ending) | 87 | 8% |
| [The Ogre](silver_scrolls/the_ogre.md) | the advisors, at the ogre's pass | a beach ball, then a heal miracle dispenser by the temple | 59 | 5% |

The creature breeder (Land 1 and Land 4) is in the shared table below.

## Land 2

Khazar's land starts the singing stones and the beach temple at once. The others wait on towns: the control script
watches each quest's town from the start, and each quest opens when the player owns its town (the plague also waits for a
finished worship site, and so does the sacrifice; the sea needs two towns, the idol its town and more than five towns in
all).

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [The Singing Stones (land 2)](silver_scrolls/the_singing_stones_land_2.md) | a priest at the hut above the stones | none: each tune's effect (night with bats, the dead raised, snow) is the prize | 88 | 1% |
| [The Beach Temple Puzzle](silver_scrolls/the_beach_temple_puzzle.md) | a Greek farmer living in the beach temple | the temple heals every living thing within 10 of it, for good | 51 | 2% |
| [The Plague](silver_scrolls/the_plague.md) | a sick Indian trader of the poisoned village | the first lightning miracle enabled at the village | 57 | 3% |
| [The Riddles](silver_scrolls/the_riddles.md) | Annika, a woman of an Indian village | a zebra creature to swap to | 52 | 5% |
| [The Slavers](silver_scrolls/the_slavers.md) | a villager of the Greek village by Khazar, then the slavers' chief | a wolf-pack miracle dispenser, and the freed slaves and the slavers join the village | 115 | 5% |
| [The Sacrifice](silver_scrolls/the_sacrifice.md) | an Indian priest and priestess, at the player's worship site | a heal miracle chest, for reaching 4000 prayer power with the family alive | 76 | 3% |
| [The Sea](silver_scrolls/the_sea.md) | a mother at her hut in the Indian town by the bay | a "big" creature miracle dispenser, if all five sons walk home alive | 84 | 0% |
| [The Greedy Farmer](silver_scrolls/the_greedy_farmer.md) | a Celtic farmer on the edge of his town | the lightning miracle and its second level for the town | 143 | 0% |
| [The Idol](silver_scrolls/the_idol.md) | the good advisor, at the idol in the hills | the fireball miracle's first and second levels for the nearest town | 116 | 2% |

## Land 3

Lethys's land starts both quests as the player arrives through the vortex.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [The Rejuvenator](silver_scrolls/the_rejuvenator.md) | an old woman at a lone Celtic hut | an ape creature to swap to (a chimp if the creature already is an ape) | 60 | 4% |
| [The Shaolin](silver_scrolls/the_shaolin.md) | a guru at the mountain temple above the Japanese village | a Wonder begun for the player during the creature's rescue | 67 | 3% |

## Land 4

The ruined land starts the fish puzzle and the breeder at the beginning; the blind woman's quest waits until the player
owns the Aztec village.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [The Fish Puzzle](silver_scrolls/the_fish_puzzle.md) | a boy turtle farmer on the shore | a tortoise creature to swap for | 44 | 3% |
| [The Treacherous Path](silver_scrolls/the_treacherous_path.md) | a woman at her hut in the Aztec village | a wolf creature to swap for, if her brother is healed | 62 | 7% |

## Land 5

Nemesis's land starts the wolf puzzle, the traitor and the crusaders at the beginning, and the returning explorers too if
their ark sailed on Land 1. The brown bear waits for the player to win the neutral Japanese village, the heavenly fire
for the first win of the Greek village.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [Stanley The Wolf](silver_scrolls/stanley_the_wolf.md) | the wolf's owner, by a campfire beside the puzzle | a lion creature to swap for, and the fireball's second and third levels | 53 | 2% |
| [The Japanese Traitor](silver_scrolls/the_japanese_traitor.md) | a stranger praying at a fire in the forest, at night | none: secrets about Nemesis's curse on the creature | 42 | 5% |
| [The Magic Dragon](silver_scrolls/the_magic_dragon.md) | the crusaders' leader, by the dragon's cave | a flying-flock miracle dispenser | 69 | 10% |
| [The Explorers Again](silver_scrolls/the_explorers_again.md) | three sailors by their wrecked boat | a polar bear to swap for, and a ready-built Norse town | 43 | 4% |
| [Swap To Brown Bear](silver_scrolls/swap_to_brown_bear.md) | a Japanese farmer on the edge of the neutral Japanese village | a brown bear to swap to, and a heal miracle chest | 55 | 2% |
| [The Heavenly Fire](silver_scrolls/the_heavenly_fire.md) | none: it starts by itself, announced by the evil advisor | an itchy creature miracle seed, if the village is well protected | 61 | 2% |

The heavenly fire puts up no scroll (that part of its script is switched off), but it is recorded in the challenge log
like the other silver quests, so it is counted here.

## On more than one land

| Quest | Lands | What it is | Rows | Score |
|-------|-------|------------|------|-------|
| [The Creature Breeder](silver_scrolls/the_creature_breeder.md) | 1 and 4 (also placed on 2 and 5 but never started there) | a standing silver scroll over the breeder's kennels, offering the special creatures to swap for; it comes back forever | 31 | 2% |
| [Creature swaps](silver_scrolls/creature_swaps.md) | all | not a quest: the shared offer that ends every quest whose reward is a creature, the breeder's stricter version of it, and the five swap scrolls cut before release | 46 | 0% |

## Cut or never started

Written for the game but never run by it; every row in these files is n/a.

| Quest | Land | Rows | Why it never runs |
|-------|------|------|-------------------|
| [Swap To Cow](silver_scrolls/swap_to_cow.md) | 4 | 31 | compiled into the shipped game, but no land's control script or any other script starts it |
| [Creature Ladder](silver_scrolls/creature_ladder.md) | none | 24 | never compiled; it has no title or lines in the text table and its tattoo reward was never written |
| [The Big Whale](silver_scrolls/big_whale.md) | 1 | 19 | never compiled; none of its lines are in the text table |
| [The Sculptor (cut silver version)](silver_scrolls/the_sculptor_cut_silver.md) | 1 | 22 | never compiled; replaced by the gold-scroll Sculptor of the story, and even the early control script that names it could not start it |
| [The Nomads (Food for Thought)](silver_scrolls/food_for_thought.md) | 2 | 22 | never compiled; only its title is in the text table |
| [Landslide](silver_scrolls/landslide.md) | 1 | 15 | never compiled; neither its title nor its lines are in the text table |
| [The Attackers](silver_scrolls/the_attackers.md) | 2 most likely | 66 | missing from the compiler's project list, so not in the compiled program; nothing starts it |
| [The Miracle Stones](silver_scrolls/the_miracle_stones.md) | 2 | 72 | missing from the compiler's project list, so not in the compiled program; nothing starts it |
| [See The Citadel](silver_scrolls/see_the_citadel.md) | 1 | 41 | never compiled; an early draft of the opening at the temple, a tutorial rather than a silver scroll |
| [Chimp Posse](silver_scrolls/chimp_posse.md) | 5 | 43 | never compiled; it opens a challenge record but puts up no scroll and gives no reward |

Two more cut scrolls live inside other files: the five cut swap scrolls (horse, leopard, lion, tortoise, wolf) in
[creature_swaps.md](silver_scrolls/creature_swaps.md), and the Slavers Warning, a bronze scroll that is compiled but
never started, at the end of [the_slavers.md](silver_scrolls/the_slavers.md).

## Not silver scrolls

Some challenges look like silver scrolls but are not counted here:

- Lethys's vortex on Land 2 is logged as a step of the gold story, not as a silver quest (see [land_2.md](land_2.md)).
- Khazar's Fireball and Shield challenges on Land 2 are gold scrolls (see [land_2.md](land_2.md)).
- The bronze puzzles are signposted by information signs, not silver scrolls: the tree puzzles of lands 2 and 3, and the
  Theseus maze and the Japanese totem puzzle of land 4 (see [minigames.md](minigames.md)).
- The Spiritual Healer on Land 2 has no scroll, no highlight and no log entry; it is a village event, kept in
  [silver_scrolls/the_spiritual_healer.md](silver_scrolls/the_spiritual_healer.md) (63 rows, 2%) for completeness.
- Land 3's man who wants to be thrown ("throw the bloke") is a standing joke with no scroll or log entry (see
  [minigames.md](minigames.md#the-man-who-wants-to-be-thrown-land-3)).

## Totals

| Land | Silver quests | Rows | Score |
|------|---------------|------|-------|
| Land 1 | 9, plus the breeder | 629 | 5% |
| Land 2 | 9 | 782 | 2% |
| Land 3 | 2 | 127 | 3% |
| Land 4 | 2, plus the breeder | 106 | 5% |
| Land 5 | 6 | 323 | 4% |
| The breeder (lands 1 and 4) | 1 | 31 | 2% |
| All lands | 29 | 1998 | 4% |

The all-lands line counts the 28 single-land quests and the breeder once: 139 partial, 1758 todo and 101 n/a rows, no
row done yet. The shared swap offer (46 rows, 0%), the Spiritual Healer and the cut quests are left out of it.

## Known openblack blockers

None of the silver quests runs in openblack yet, for the same few reasons:

- The land control scripts never reach them. Land 1's control script stops at its first unwritten command during the
  land's set-up, long before any silver quest is started, and the control scripts of Lands 2 to 5 never run, because the
  story always begins with Land 1.
- Scripts can only make two kinds of object: making an object from a script handles scenery objects and rocks, so the
  quests' villagers, creatures, animals, dispensers, puzzles, reward chests and scrolls never appear.
- Reading the game time from a script returns no value at all, which leaves the script's stack short; the shared reward
  script and several quests read it.
- Stopping a script by name compares the whole text with each script's name. The game is reported to split the text
  into several names at spaces, commas and tabs; this comes from the executable and has not been checked yet.
- Most of the commands the quests need (dialogue, advisors, camera moves, highlights, the challenge log, rewards,
  timers, villager states) are still stubs in `src/CHLApi.cpp` that only log "not implemented".
