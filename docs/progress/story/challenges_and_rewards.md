# Challenges and rewards

How a challenge runs across the lands: it is found by its scroll, recorded when started and finished with how well it
went and how good or evil the player was, and pays out a reward. The scrolls themselves are in
../interface/scrolls_and_signs.md.

Every silver scroll, land by land, is listed in [silver_scrolls.md](silver_scrolls.md); the shared creature swap is in
[silver_scrolls/creature_swaps.md](silver_scrolls/creature_swaps.md) and the breeder in
[silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md).

**Progress: 0/23 done, 1 partial — 2%**

## Running a challenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge starts when its scroll is tapped and the advisors announce it | todo | see ../interface/scrolls_and_signs.md |
| Gold scrolls are the story and must be done; silver ones are optional | todo | see ../interface/scrolls_and_signs.md; every silver scroll: [silver_scrolls.md](silver_scrolls.md) |
| Bronze "did you know" scrolls give tips and some hide puzzles | todo | see ../interface/scrolls_and_signs.md |
| A challenge can be set to wait until another is finished | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp`; e.g. [The Ogre](silver_scrolls/the_ogre.md) waits for the guide's fight lesson, and [The Shaolin](silver_scrolls/the_shaolin.md)'s Wonder comes only if it was done before the creature's rescue |
| A started challenge is recorded with a title, a reminder and a picture | todo | the snapshot command is a stub |
| Its record is updated with how well it went (0 to 1) and the alignment it earned | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Tapping the scroll again replays the challenge's reminder | todo | see ../interface/scrolls_and_signs.md |
| The challenge room shows every recorded challenge | todo | see ../temple/challenge_room.md |
| A challenge can be replayed from the challenge room | todo | see ../temple/challenge_room.md |
| What the player did in a challenge moves their alignment | todo | see ../worship/alignment.md |
| Each time a challenge's record is updated, its alignment (−1 to 1) moves the player's alignment, scaled by a factor of the player's own | todo | the factor's value is not determined; seen in [The Pied Piper](silver_scrolls/the_pied_piper.md) and [The Sea](silver_scrolls/the_sea.md); see ../worship/alignment.md |

## Rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest falls from the sky with a trail of dust | todo | the reward commands are stubs; e.g. the toy ball of [Throwing Stones](silver_scrolls/throwing_stones.md) |
| A reward can appear in a town, or without falling | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Opening a reward chest gives what it holds | todo | e.g. the large food reward of [The Lost Flock](silver_scrolls/the_lost_flock.md) |
| The first reward is explained by the advisors | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| A miracle dispenser as a reward, with how many charges and how often it refills | todo | see ../miracles/dispensers_and_seeds.md; e.g. [The Singing Stones](silver_scrolls/the_singing_stones.md) (food), [The Hermit](silver_scrolls/the_hermit.md) (water); every dispenser reward is listed in [rewards.md](rewards.md) |
| One-shot miracle seeds as a reward | partial | seeds exist (../miracles/dispensers_and_seeds.md); the script command that makes them is a stub |
| A new creature to swap for | todo | the swap command is a stub; see ../creature/; the shared offer: [creature_swaps.md](silver_scrolls/creature_swaps.md); e.g. the sheep of [The Lost Flock](silver_scrolls/the_lost_flock.md), the tortoise of [The Fish Puzzle](silver_scrolls/the_fish_puzzle.md), the wolf of [The Treacherous Path](silver_scrolls/the_treacherous_path.md) |
| Gate stones and keys that open the way | todo | see land_1.md |
| Rewards are signposted with a bronze scroll | todo | see ../interface/scrolls_and_signs.md |

## Creature swaps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The breeder offers creatures; the player confirms with the Action button on the one they want | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp`; see [the_creature_breeder.md](silver_scrolls/the_creature_breeder.md) |
| The new creature keeps what the old one learnt | todo | (unconfirmed which parts carry over) |
| Swap to an ape, a cow, a brown bear and other species through the story | todo | titles exist for swaps to cow, horse, leopard, turtle, wolf, chimp and brown bear; only some are used; the cut swap scrolls: [creature_swaps.md](silver_scrolls/creature_swaps.md#cut-swap-scrolls-never-in-the-game) |
