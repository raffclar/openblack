# Special villagers

Most villagers are nameless members of their tribe. A few are not: now and then a new villager is born or made with a
real person's name floating over its head (from the game's own list of 48, or from the player's address book), the
story's characters are special kinds of villager with their own models (the piper, the hermit, the sculptor, the
monk, the sailors and the rest), the opening family are drawn in high detail, and missionary disciples move into other
towns to preach. Ordinary looks and voices are in [looks_and_voices.md](looks_and_voices.md), disciples in general in
[disciples.md](disciples.md), and each story character's challenge in its land's file
([land 1](../story/land_1.md), [land 2](../story/land_2.md), [land 3](../story/land_3.md),
[land 4](../story/land_4.md), [land 5](../story/land_5.md)).

**Progress: 0/77 done, 3 partial — 2%**

## The table of names

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's info table holds 48 named people (developers and friends of the studio), each with a name of up to 47 characters, an age, a sex, a job, whether married, a tribe, a pet animal and a face number | partial | read into `src/InfoConstants.h` (48 entries); nothing uses them |
| The table is built once at start-up | todo | |
| When the player's address book gives contact names, they fill the table first and the game's own names only fill what is left up to 48; with 48 contacts or more only contacts are used | todo | see ../pc_integration/villager_names_from_contacts.md for how contacts are read |
| A contact's entry gets an age of 11 to 50, sexes alternating male then female in the order read, no job, married at random, any tribe and a random pet number | todo | see ../pc_integration/villager_names_from_contacts.md |
| The table has room for 148 names | todo | |
| In a multiplayer game only the 48 built-in slots count | todo | |
| Ordinary villagers have no name at all; only named villagers answer with one | todo | the "every villager has a name" row in looks_and_voices.md does not match the game |

## Becoming a named villager

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whenever the game makes a villager (born, made by a script or a building, or thrown from a vortex), it rolls a number from 0 to 9; on 0 or 1 (2 in 10) it tries to make a named villager instead | todo | |
| Named villagers are never made in a multiplayer game | todo | |
| The name is picked at random among the names of the villager's sex that are not yet in use in this land, by walking the table from a random place | todo | the job is not checked |
| When every name of that sex is in use, an ordinary villager is made instead | todo | |
| A named villager takes its name's age, unless it is being born (age 1) | todo | |
| Each name is used by at most one villager per land; the counts are cleared when the land is cleared | todo | |
| The name's job, marriage, tribe, pet and face are not used when it is chosen (unconfirmed whether anything else uses them) | todo | |
| A named villager otherwise lives, works, breeds and dies like any villager of its kind | todo | |
| A named villager is saved and loaded with its name | todo | |
| A script can ask for a named villager of a given town (the command exists but no land script uses it) | todo | `src/CHLApi.cpp` logs it as not implemented |

## Showing the name

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The name floats over the villager's head (over its head bone when it has a model, otherwise over its position) | todo | |
| The name is white, or the colour of the player who owns the villager's town | todo | |
| Names show while the "show villager names" display is up (it fades in and out); villagers marked to always show their name show it anyway | todo | the key binding exists (`src/Gui/GameMenu.cpp`); exact link between the key and the fade unconfirmed |
| No name is shown for a villager a script is controlling | todo | |
| No name is shown while the game is in a mode that hides them, or for a villager marked hidden (unconfirmed which mode) | todo | |
| A villager whose speech bubble is up shows the bubble instead of its name | todo | |
| Names are drawn in the depth-sorted pass with the other see-through things | todo | |
| openblack has a debug overlay that labels villagers with numbers and states | n/a | `Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`; a debug tool, see ../debug/debug_windows.md |

## Speech bubbles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A named villager can say something in a speech bubble over its head | todo | |
| Only one villager speaks at a time; a new line moves the bubble to the new speaker | todo | |
| The bubble is 250 by 175 with text size 19 and holds up to 1023 characters | todo | |
| The bubble sits 1.5 above the head and closes after 6 seconds | todo | |
| The bubble closes when the villager dies, is deleted or is hidden | todo | |
| New e-mail is announced by a named villager in its bubble ("Hey! I've sent you an e-mail about ..." when the sender is that villager's contact, otherwise "Hey! ... has sent you an e-mail about ...") | n/a | the game's mail link is gone; see ../pc_integration/villager_names_from_contacts.md |

## High-detail villagers (the opening family)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can turn a villager into a high-detail one and back | todo | `SetHighGraphicsDetail` in `src/CHLApi.cpp` is a stub |
| The father, mother and son of the opening use their own high-detail models; every Celtic and Norse boy uses the son's | todo | see ../story/tutorial.md |
| The creature trainer who offers the first creatures uses his own high-detail model | todo | |
| High-detail models have the same 22 bones as villagers, so the villager animations drive them | todo | |
| They cross-fade between animations over 300 ms and smooth their turning | todo | |
| Their eyes are separate eyeballs with four lids; they blink every 1 to 5 seconds at random and glance together | todo | |
| They are lit by the default sun with no haze | todo | |
| They stay high detail only while a script holds the wide-screen cinema; otherwise they are released at the end of the frame | todo | |
| A swimming high-detail villager is drawn cut by the water's surface and leaves a ring every second | todo | see ../ocean/ |
| They cast projected shadows | todo | see ../rendering/ |
| A script can give a high-detail villager special orders in a cinematic (follow the opening hand's grip, snap to it, mirror its turn, turn a quarter left or right) | todo | `ThingJcSpecial` is a stub |
| Every high-detail villager is released when the land is cleared | todo | |

## Special kinds of villager

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game has 21 special kinds besides the tribes' seven each: piper, monk, idol builder, hermit, hippy, priest, priestess, marauder, two footballers, engineer, shepherd, nomad, Aztec leader, creature trainer, Norse sailor, breeder, healer, sculptor, crusader and sailor with an accordion | partial | listed in `src/Enums.h` and the villager table; the editor can place one with its model (`src/Editor/EditorEntities.cpp`) |
| The special kinds are only made by challenge scripts, never by towns | todo | script creation of villagers is not done (`CreateScriptObject` in `src/CHLApi.cpp`) |
| The pied piper (Land 1): plays in his cave and leads the town's children away; he cannot be hurt; if the children die he retires | todo | see ../story/land_1.md |
| The hermit (Land 1): his hut must not be wrecked; he offers a gesture to be left alone, and the advisors argue if the player kills him | todo | see ../story/land_1.md |
| The sculptor (Land 1): wants a particular rock from the old quarry brought to him to carve the cow gate stone, the last of the creature-gate stones | todo | see ../story/land_1.md and [../story/gold_scrolls/the_sculptor.md](../story/gold_scrolls/the_sculptor.md) |
| The hippy (Land 1): wants the strongest shaking magic mushroom for his experiment; also at the singing stones | todo | see ../story/land_1.md and ../story/minigames.md |
| The creature trainer (Land 1): offers the choice of creature and turns up in the creature's lessons | todo | see ../creature/species_choice.md |
| The Norse sailors and the accordion sailor (Land 1): the missionaries who build their ark from the wood the player gives, sing three verses and sail away; they return in Land 5 with an offering | todo | see ../story/land_1.md and ../story/land_5.md |
| The priest and priestess (Land 2): offer a sacrifice as a gift; a priest is also the spiritual healer | todo | see ../story/land_2.md |
| The engineer (Land 2): comes out of the workshop to explain it | todo | see ../story/land_2.md and ../building/workshop_and_scaffolds.md |
| The idol builder: a special kind the game uses only on Land 5, among the villagers who wave the creature off at the end | todo | see ../story/ending.md; Land 2's idol maker is an ordinary Indian trader, see [the_idol.md](../story/silver_scrolls/the_idol.md) |
| The monk (Land 3): walks back to his temple if followed; part of taking the Japanese town | todo | see ../story/land_3.md |
| The nomad (Land 4): lives in his shack and holds one of the meteorite quests | todo | see ../story/land_4.md |
| The crusaders (Land 5): march in the crusaders' and traitor's challenges | todo | see ../story/land_5.md |
| The footballers (a Norse and a Celtic team): both walk on in the shipped Land 5 end credits, a Celtic footballer among the ten waving villagers and a Norse one in the special shepherd's place; otherwise only in a cup-final challenge that is not compiled into the game | todo | see [../town/football.md](../town/football.md#the-cup-final-and-the-footballers) and ../story/ending.md; the cup final: ../story/minigames.md |
| The marauder, Aztec-leader actor, breeder and healer only appear in the end credits; the special shepherd never appears: the credits put a Norse footballer in his place | todo | see ../story/ending.md and [../town/football.md](../town/football.md#the-cup-final-and-the-footballers) |
| In the end credits the special characters walk on one by one beside the names | todo | see ../story/ending.md |

## Story characters played by ordinary villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lost brother and his sister (Land 1): she gives the gate key when he is found in the trees through the pass | todo | see ../story/land_1.md |
| The shepherd of the lost flock (Land 1): has 5 sheep and pays when they are brought back, worse off for each one that dies | todo | see ../story/land_1.md |
| The shepherd sucked into the first vortex who comes back out (Land 1) | todo | see ../story/portals.md |
| The family on the beach (Land 2) whose children must be saved from the sea | todo | see ../story/land_2.md |
| The greedy farmer (Land 2): wants the cow thieves stopped, not killed | todo | see ../story/land_2.md |
| The woman with the old scroll of clues (Land 2): gives the treasure clues once her house is fixed, and repeats them when her door is knocked | todo | see ../story/land_2.md |
| The slavers (Land 2) who march down to attack a town | todo | see ../story/land_2.md; [the_slavers.md](../story/silver_scrolls/the_slavers.md) |
| The man who wants to be thrown (Land 3) | todo | see ../story/land_3.md and ../story/minigames.md |
| The blind woman (Land 4): carries potions to cure her brother, using one each time she is hurt; the player watches over her journey | todo | see [the_treacherous_path.md](../story/silver_scrolls/the_treacherous_path.md) |
| The ogre (Land 4): a champion who holds a guardian stone and must be fought | todo | see ../story/land_4.md |
| The Japanese traitor (Land 5) | todo | see ../story/land_5.md |
| The creature breeder (Lands 1, 2, 4, 5): offers other creatures to swap for | todo | see ../creature/species_choice.md |

## Missionaries

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A missionary disciple goes to the nearest building of any town and joins that town as a missionary of its own player | todo | `src/ECS/Systems/Implementations/LivingActionSystem.cpp` has the state as a to-do entry; see disciples.md |
| It is moved into that building and changes there | todo | |
| With no building anywhere it goes back to deciding what to do | todo | |
| The town keeps its missionaries in a list with their count, saved with the game | todo | |
| A missionary's impressiveness is the land's missionary balance value times a value of its player (unconfirmed which) | todo | |
| Villagers near the missionary react to it; the pull falls off with distance up to the reaction's range and with boredom, and wins belief for the missionary's player | partial | the missionary reaction is listed in `src/ECS/Systems/Implementations/ReactionSystem.cpp`; nothing makes missionaries |
| Missionary disciples go home when their town changes owner (see disciples.md) | todo | |
