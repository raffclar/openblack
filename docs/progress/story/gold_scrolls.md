# Gold scrolls

The gold scrolls are the story: the quests the player must finish to move through the lands. Each one is started by its
land's control script and logged in the story's challenge log. The optional quests are the silver scrolls
([silver_scrolls.md](silver_scrolls.md)).

This is an index, not a scored file. Each quest's own file in [gold_scrolls/](gold_scrolls/) holds its rows; until a
quest has one, its only coverage is the single summary row in its land file.

## Where things stand (2026-10-08)

- **No gold quest runs in openblack yet.** Every land's control script stops at a script command that isn't written,
  and the commands each quest needs (dialogue, scrolls, advisors, rewards, villager and creature control) are still
  stubs in `src/CHLApi.cpp`. See [silver_scrolls.md](silver_scrolls.md#known-openblack-blockers) for the blockers both
  kinds share.
- **Every gold quest now has its own file** in [gold_scrolls/](gold_scrolls/) (32 files, cut quests included), plus [creature_guide.md](creature_guide.md) for the guide's part of Land 1 and [tutorial_island.md](tutorial_island.md) for the tutorial's six lessons. All score 0–13%: nothing is done.
- Losing ([losing_and_game_over.md](losing_and_game_over.md)) and the land-to-land vortex ([portals.md](portals.md)) have their own files. Bugs in the original quests are collected in [../vanilla_bugs/](../vanilla_bugs/).

Coverage: **summary** = only the land file's one row; **file** = its own quest file.

## Land 1

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| The Temple Is Finished (no title in the game) | the two advisors, at the new temple | none (it opens the way to the gold scrolls) | The advisors show the finished temple's entrance, make the player go in and out, and show a stand-in gold scroll where Choose Your Creature's will appear | [file](gold_scrolls/the_temple_is_finished.md), 38 rows | 12% |
| Choose Your Creature | Sable, the creature trainer, at the creature gates | the player's creature (cow, ape or tiger) | Sable the trainer asks for three gate stones (tiger, ape, and the cow stone carved from a blank rock); the gates open, the cow, ape and tiger show off and the player picks one | [file](gold_scrolls/choose_your_creature.md), 94 rows | 7% |
| The Lost Brother | the brother's sister, a Norse housewife, at her house | the ape gate stone | A woman's feverish brother has wandered off; bringing him to her earns the ape gate stone (healing, smashing her house or killing either changes the ending and alignment) | [file](gold_scrolls/the_lost_brother.md), 73 rows | 7% |
| The Sculptor | the sculptor, outside his house | the cow gate stone | The uncarved rock from the quarry is carved by the village sculptor into the cow gate stone | [file](gold_scrolls/the_sculptor.md), 51 rows | 13% |
| The Creature's Learning (the trainer) | Sable, at the creature pen by the temple | none (the lessons hand over the three leashes) | Sable's five lessons: the new home and stroking, eating, punishment, the Leash of Learning, tying the leash with the aggression and compassion leashes | [file](gold_scrolls/the_creatures_learning.md), 145 rows | 6% |
| The Creature's Learning (the guide) | the guide, a huge creature wandering the valley | none | The guide meets the creature, teaches it to impress a village and to fight, and dies in the storm | [file](creature_guide.md) | 5% |
| Leave Through the Vortex (no title in the game) | the two advisors, at the vortex near the coast | the way to the second land | A golden light, then a vortex that swallows and returns a shepherd; the scroll over it dives the camera in and ends the land | [file](gold_scrolls/leave_through_the_vortex_land_1.md), 42 rows (vortex: [portals.md](portals.md)) | 2% |

## Land 2

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| Worship Site | Khazar, at scrolls by the player's temple | a builder disciple, and the worship site's grain, wood and water miracles | A builder disciple raises the worship site, then Khazar teaches the totem and charging miracles; he places the workshop scaffold and the "Miracle Challenge" scroll | [file](gold_scrolls/worship_site.md) (77 rows) | 4% |
| The Workshop | an engineer "sent by Khazar", at the workshop | the Forest miracle; the engineer joins the village | Give the workshop wood to make a scaffold, place it, house the homeless | [file](gold_scrolls/the_workshop.md) (65 rows) | 5% |
| Impress Village | Khazar, at a scroll west of the home village | none | 20 minutes after the worship lesson: a gesture demo and practice, then fill the nearest Norse village's emptied store (over 2,800 food); winning that village brings Khazar's lesson on influence (not a scroll, in the same file) and the land's last scroll | [file](gold_scrolls/impress_village.md) (70 rows) | 6% |
| Khazar's Fireball Challenge | Khazar, through the "Miracle Challenge" scroll, then a scroll on the target huts | the Fireball miracle at the home worship site | Khazar's tour, then throw fire seeds at three target huts, up to three rounds | [file](gold_scrolls/khazars_fireball_challenge.md) (73 rows) | 4% |
| Khazar's Shield Challenge | Khazar, at a scroll on an island hut | the Physical Shield miracle at the home worship site | A shield demo, then three shield seeds against Khazar's own three boulders (not a Lethys attack) | [file](gold_scrolls/khazars_shield_challenge.md) (65 rows) | 9% |
| Nemesis. No! (Khazar's death) | none (a story film; Nemesis speaks) | none | No scroll, a logged story film: Nemesis burns Khazar's town, blows up his temple and kills his creature; Lethys's creature takes the Creed. Set off by the first town Lethys loses, the player's sixth town or Khazar losing his towns | [file](gold_scrolls/nemesis_no.md) (66 rows) | 9% |
| Destroy it! (the land's last scroll) | the evil advisor, at a scroll on the shore | none | A fly-over of Lethys's three towns; the log tracks the share of them he has lost | [file](gold_scrolls/destroy_it.md) (50 rows) | 2% |
| Lethys has taken our Creature! | none (a story film at Lethys's temple) | none | No gold scroll, a logged story film: Lethys walks the creature into his vortex; a silver "follow now" scroll gives about 10 seconds to follow straight to Land 3 | [file](gold_scrolls/lethys_has_taken_our_creature.md) (55 rows) | 9% |
| Leave through the vortex | the good advisor, at a scroll over the vortex | none (the way to Land 3) | Once Lethys has no towns and his temple is at a tenth of its health; no title, no log entry (vortex: [portals.md](portals.md)) | [file](gold_scrolls/leave_through_the_vortex_land_2.md) (29 rows) | 4% |

## Land 3

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| So You Couldn't Bear to Be Without Your Creature? (free the creature) | the advisors, on arrival (no scroll) | the creature back, the creed from Khazar's creature and the exit vortex | Three prison pillars hold the creature, one per village; each village won sinks its pillar, each lost raises it; Lethys then begs, gives the creed and opens the vortex, and may be spared or finished | [file](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md) (101 rows) | 2% |
| The Wolves Are Possessed | Lethys (a cut scene; no scroll) | none | Lethys's 20 possessed wolves attack the Indian village (after the second pillar); the monk turns all but 8 into cows if his quest is done; a log entry, no scroll, no reward | [file](gold_scrolls/the_wolves_are_possessed.md) (28 rows) | 0% |
| Fire! Fire! I'm on Fire! | Lethys (a cut scene; no scroll) | saved fishermen join the village; alignment +1 / 0 / −0.8 by how many are saved | Lethys sets 16 fishermen alight (after the Japanese village is won first); put them out before they burn; the monk may give two water miracles | [file](gold_scrolls/fire_fire_im_on_fire.md) (46 rows) | 10% |
| Leave through the vortex | Lethys opens the vortex; the evil advisor points out the scroll | the way to Land 4 | Clicking the scroll dives into the vortex and ends the land (vortex: [portals.md](portals.md)) | [file](gold_scrolls/leave_through_the_vortex_land_3.md) (14 rows) | 4% |

## Land 4

The land's control script waits for the three Guardian Stones (lightning, darkness, fire), broken by the first three
quests below in any order, then runs the Undead Village's scene, the Creed and the vortex.

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| The Defending Ogres | a Japanese farmer at the player's temple (the story); Sleg in his lair (the fight) | the lightning Guardian Stone breaks; the lightning and rain stop | The land's opening (the ruined arrival, the three meteor curses, the town-building advice), then the farmer's story, which logs this scroll; Sleg the ogre fights the creature in his lair while his four gremlins attack it | [file](gold_scrolls/the_defending_ogres.md), 83 rows | 10% |
| The Totem Puzzle | the Japanese village's elder, at five bell towers | the fire Guardian Stone breaks; the fireballs stop | Once the village is owned, destroyed or emptied: a memory game, four rounds of 3, 5, 7 and 9 rings to click back with less time each round, retried freely | [file](gold_scrolls/the_totem_puzzle.md), 68 rows | 5% |
| The Heartbroken Man | a woman at a hut below Adam's mountain | the darkness Guardian Stone breaks and time runs again; influence | After the Totem Puzzle's introduction: bring Keiko from the Aztec village to Adam; reuniting them logs alignment +0.8, killing Keiko −0.8, killing Adam 0 | [file](gold_scrolls/the_heartbroken_man.md), 78 rows | 7% |
| Undead Village | a man of the Japanese village, under a scroll at the village's edge | the second Creed, which only the creature can pick up | After all three stones are broken: Nemesis has cursed a town into skeletons; raising both sunken totems lifts it, and the freed man says the Creed lies in the Guide's body | [file](gold_scrolls/undead_village.md), 51 rows | 6% |
| Leave Through the Vortex (no title in the game) | Nemesis (the vortex west of the home village) | the way to the fifth land | After the Creed scene's rumble: rumbles grow near a hidden spot, Nemesis opens the vortex and dares the player; clicking its gold scroll dives in and ends the land; no log entry | [file](gold_scrolls/leave_through_the_vortex_land_4.md), 36 rows (vortex: [portals.md](portals.md)) | 3% |

## Land 5

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| I have a surprise for you. (the curse) | Nemesis, in the opening film; lifted by a crusader-dressed man at the healing place | the creature restored to how it arrived, and a powered-up heal miracle dispenser | No scroll, a logged story chapter: Nemesis curses the creature; his three Wonders sap its strength, size and alignment once a night; each Wonder village won adds a third towards lifting it | [file](gold_scrolls/i_have_a_surprise_for_you.md), 85 rows | 5% |
| Nemesis's Shielded Village (no title, not logged) | none (the evil advisor warns of the shield) | none | The Tibetan village's spiritual shield, held up by three stones and their worshippers, must fall before the village can be won (the curse's third step); also Nemesis's meteors and shields at his last village and his blast barrages after the cure; no scroll, no log entry | [file](gold_scrolls/nemesis_shielded_village.md), 51 rows | 9% |
| So this is a fight to the death | Nemesis (once all his towns are taken) | the end of the game | Winning Nemesis's last village logs this quest: his mirror creature is made and fought (retries allowed), his farewell, the creature walks into the volcano, his temple falls, the burnt creature must be healed, then the credits and the crowd scene ([ending.md](ending.md)) | [file](gold_scrolls/so_this_is_a_fight_to_the_death.md), 83 rows | 6% |

## Tutorial island

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| The six lessons | the Island Keeper | none | Six gold scrolls by the start, one per lesson; tapping one runs it again | [file](tutorial_island.md) | 5% |

## Cut or never started

Gold quests that no land starts: never compiled into the shipped `challenge.chl`, or compiled but run by nothing. Their
rows are n/a, so they don't score. The first land's unused trainer lessons (the good-and-evil leash lesson, which is
compiled but never run, and the uncompiled leash, thirst and toilet drafts) are listed in the "Unused material" of
[the_creatures_learning.md](gold_scrolls/the_creatures_learning.md); the cut opening at the temple, which carries a gold
scroll, is in [silver_scrolls/see_the_citadel.md](silver_scrolls/see_the_citadel.md); the unused mirror-creature fight
is in [so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md).

| Quest | Giver | Reward | What happens | Coverage | Score |
|---|---|---|---|---|---|
| The Madness | none (a cut scene that starts by itself) | none | Land 5 by its markers: Nemesis makes the creature strong, flips its alignment and has it fireball a Japanese village's store and houses, then it is confined for 90 seconds; title and lines are in the text table; the shipped curse replaced it | [file](gold_scrolls/the_madness.md), 29 rows | n/a |
| Release the Creature (title missing) | none (a cut scene when the camera finds the creature) | none (the creature itself) | The creature tied to a boulder in a hilltop prison, watched by a hostile cow creature; hitting the boulder with rocks frees it; no text was ever written, and it waits on a flag nothing sets; the shipped third land's rescue replaced it | [file](gold_scrolls/release_the_creature.md), 18 rows | n/a |
| The Ally Speaks (no title) | none (its gold scroll is commented out) | none | A camera-path cut scene at the second land's player temple with fifteen unwritten lines; its gold scroll is commented out | [file](gold_scrolls/the_ally_speaks.md), 11 rows | n/a |
| Villager Catch (no title) | a gold scroll over the town | none (only the advisors' comments on the score) | A god flings ten villagers out of a town one by one; catch them; the advisors rate the score; no text was ever written | [file](gold_scrolls/villager_catch.md), 20 rows | n/a |
| Choose Your Creature (BAFTA draft) | the gold scroll at the creature gate | the chosen creature | A shortened draft that skips the gate-stone hunt, started only by an uncompiled demo control script | [file](gold_scrolls/choose_your_creature_bafta_draft.md), 18 rows | n/a |
