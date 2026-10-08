# Unused content

What was made for the game but never reaches the player: challenges whose scripts were never compiled into the shipped
challenge file, scenes cut from shipped challenges, creature actions that do nothing, and data files nothing loads.
"Unused" here was checked by comparing the script sources of the Mac release (the original sources, kept with the
challenge compiler work in openblack's research folder) against the 514 scripts actually inside
`Scripts/Quests/challenge.chl`. Unused puzzle kinds and the best-known cut games (cow bowling, whack-a-villager, the
cup final and so on) are listed in [../story/minigames.md](../story/minigames.md); they are not repeated here.

**Progress: 0/2 done, 0 partial — 0%**

## Challenges never compiled into the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Creature Ladder: fifteen fights on plinths against copies of your own creature in every shape, in order sheep, chimp, cow, ape, zebra, horse, mandrill, tortoise, wolf, gorilla, leopard, bear, polar bear, tiger and lion | n/a | not in the shipped challenges |
| The Marauders: a pack of ten raids the village three times, first driving off a flock of cattle to their den, then wrecking the storage pit, then the crèche; they retreat from a raid once two are killed, and leaving food at their camp keeps them away for a while (advisor: "That food should keep the Marauders out of town for a bit longer.") | n/a | not shipped |
| The Caretaker: a good and an evil version of an adventure with a following of chickens, marauders and an "avatar" | n/a | not shipped; the source is only an outline |
| The Chimp Posse: three chimp creatures guard their toys (a die and a teddy) and quarrel with Nemesis' creature over a toy ball ("Nemesis' Creature looks mighty pissed.") | n/a | not shipped |
| The Big Whale: a whale has swallowed a fisherman's son, and a rock thrown at it makes it spit him out | n/a | not shipped; unfinished |
| Food for Thought (poisoned and magic food for a flock), Raiding Lions, Landslide, Storm Brewing, the Spitting Totem (a totem that spits objects out), Villager Catch, The Meeting, Rebuild Village, Release the Creature, Free Fall, Make Ball, Bug Flock and Throw It | n/a | not shipped; Food for Thought is described in [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md); Villager Catch: [../story/gold_scrolls/villager_catch.md](../story/gold_scrolls/villager_catch.md); Release the Creature: [../story/gold_scrolls/release_the_creature.md](../story/gold_scrolls/release_the_creature.md) |
| The swap-to scripts for the horse, leopard, lion, turtle, wolf and sheep (only the ape, brown bear and cow swaps are compiled; the breeder uses its own swapping) | n/a | not shipped; see the secret creatures in [hidden_content.md](hidden_content.md) |
| A series of creature-training lessons (learn to eat, play and poo, the leashes one by one, drinking, thirst, meeting the guide, seeing its home, helping and impressing a town) replaced by the single shipped creature-development script | n/a | not shipped; see [../creature/lessons_and_help.md](../creature/lessons_and_help.md) |
| Test scripts: "Hello, world!" (its header: "This classic \"Hello, world!\" script re-defines the gaming experience."), a test that makes a Norse housewife at the hand, a storm-over-the-cows night scene, black cloud, food store, scrap and "did you know" tests, creature and creature-animation tests, and another developer's copy of the Land 1 control script | n/a | not shipped |
| A "BAFTA" control script that runs the old versions of the Lost Brother, the Pied Piper and Choose Your Creature, apparently for an awards demonstration | n/a | not shipped; its Choose Your Creature draft: [../story/gold_scrolls/choose_your_creature_bafta_draft.md](../story/gold_scrolls/choose_your_creature_bafta_draft.md) |
| The Madness: Nemesis takes over the player's creature, makes it strong and of the opposite alignment and sends it to throw fireballs at a village's store and houses, then it is confined for a minute and a half until the madness wears off | n/a | not shipped; the fifth land's curse does the same story beat: [../story/gold_scrolls/the_madness.md](../story/gold_scrolls/the_madness.md) |
| The Ally Speaks: a cut scene, once meant to be offered from a gold scroll at the player's temple, in which the camera flies a set path while the advisors talk about another god's temple; its lines were never written | n/a | not shipped: [../story/gold_scrolls/the_ally_speaks.md](../story/gold_scrolls/the_ally_speaks.md) |

## Cut from shipped challenges

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Lost Brother once had three more endings, all commented out: both brother and sister killed, both brought home safe, and the brother "left for dead" when his health, draining while he was lost, ran out | n/a | the advisors' lines for them exist; the shipped challenge has none of it |
| The hidden phone box's land-skipping cheat (see [hidden_content.md](hidden_content.md)) | n/a | replaced by "Sorry, the boss got the cheat removed" |
| In the unfinished Cup Final, the away side (the Celts) cheats near the end by bringing on an extra substitute | n/a | not shipped |

## Inside the program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's "watch telly" and "fart" actions exist but are empty: choosing them does nothing | n/a | nothing to port |
| "Tell a friend a joke" is fully built (it walks up fluttering its eyelids, talks, and its friend reacts) but is missing from the game's table of actions, so it can never be chosen | todo | could be added as an openblack extra; not in [secret_behaviours.md](secret_behaviours.md) for that reason |
| The developers' key menus and nine named key layouts | n/a | see [hidden_keys_and_cheats.md](hidden_keys_and_cheats.md) |
| The dance formations can lay dancers out as shapes: circle, heart, spiral, octagon, square, triangle, wavy circle, lines, random scatter, a figure eight, lightning, a star, a man, a fish, a cockerel and the letters of the alphabet (`Data/Letter*.DAN` and friends) | todo | which shipped dances use them is unconfirmed; see [../worship/worship_sites.md](../worship/worship_sites.md) |

## Data nothing loads

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `Scripts/dance.txt` is a land for editing dances, but its landscape (`dance.led`) does not ship | n/a | |
| `Scripts/comp.txt` is a computer-player test land loading a landscape from a developer folder that does not ship | n/a | |
| `Scripts/demo2.txt` sets up Land 3 with the player in the Japanese town, apparently for a demo | n/a | (unconfirmed which demo) |
| The playground "Construct" (`Scripts/Playgrounds/construct.txt`, on `construct.lnd`), announced as "Testing landscape": one Norse house, two villagers, a temple and a row of objects | n/a | not a game file: it is openblack's own test land, added to openblack's test data in 2021. It sits with the playgrounds only when copied into the install's playground folder, and then the skirmish box lists it as "construct" like any other land there; see [../multiplayer/maps/construct.md](../multiplayer/maps/construct.md) |
| Four of the hidden phone box's recordings are never played (numbers 3, 12, 13 and 14) | n/a | in the script sound bank |
| Dance files named "Test" and "OnlyDanceThatWorks" in `Scripts/Dance` | n/a | (unconfirmed whether any dance loads them) |
| A "cheat box" model (a plain box 180 × 400 × 180, sitting entirely below the ground) is in the model list; there is no bowling pin model, and the toy bowling ball and skittles are shipped toys used on the tutorial land | n/a | (unconfirmed whether the cheat box is used anywhere); the shipped toys and the cut bowling games: [../nature/toys.md](../nature/toys.md) |
| The script sources reference text for cut challenges (25 lines for the marauders, 17 for whack-a-villager, the chimps' names) | n/a | whether the shipped text still holds them is unconfirmed |
