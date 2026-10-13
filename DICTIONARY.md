# DICTIONARY

This dictionary gives the words for code comments and commit messages in openblack and bw1-decomp. These texts follow
ASD-STE100 Simplified Technical English (STE).

Use each word with the one meaning that this dictionary gives. Use the same word for the same thing every time.

The dictionary has three parts:

1. **Technical names** for Black & White. STE lets you use the technical names of your subject. These are the
   technical names of this subject.
2. **Words to use and words not to use.** These are common words in code comments. Use the word on the left. Do not
   use the words on the right.
3. **Technical verbs** for code and data.

Names of code (`PickingSystem`, `CreatureMind`, `GGame::g_game`), names of files and names of script commands
(`CREATE_HIGHLIGHT`) are technical names. Use them as they are.

## 1. Technical names for Black & White

### 1.1 The player and the hand

| Word | Meaning | Do not use |
|---|---|---|
| player | A person or a computer that plays one god in the game. | god (for the player), user |
| god | The power that a player plays. Use only in "another god", "the god's creature". | deity |
| hand | The hand that a player moves on the land with the mouse. | cursor (for the hand), pointer (for the hand) |
| pointer | The mouse position on the screen. | cursor, mouse (for the position) |
| influence | The area on the land where the player's hand can act. | reach (for this area), territory |
| influence ring | The line on the land that shows the edge of the influence. | border circle |
| pick | The thing that the pointer is on, found each frame. As a verb: to find that thing. | hit-test (as a verb), select |
| take | To take a thing into the hand. | grab, pick up (phrasal verb), grip (for this) |
| hold | To keep a thing in the hand. | carry (for the hand), grasp |
| drop | To let a thing go from the hand with no speed. | release (for this), let go (phrasal verb) |
| throw | To let a thing go from the hand with speed. | fling, toss, chuck |
| tap | A short press on a thing with the Action button. | click (for the game action) |
| gesture | A shape that the player draws with the hand to cast a miracle. | glyph, sign (for this) |
| tooltip | The text that shows near the hand when it is on a thing. | hint (for this text), popup |

### 1.2 The creature

| Word | Meaning | Do not use |
|---|---|---|
| creature | The large animal that the player teaches. | pet, titan, avatar |
| mind | The part of the creature that decides what it does. | brain, AI |
| desire | A need of the creature that makes it act (hunger, sleep, play). | want, drive |
| plan | The action that the creature decided to do next. | intent, goal (for this) |
| action | One thing that the creature does, from the action table. | behaviour (for this), task |
| learn | To change the creature's mind after an action, a stroke or a slap. | train (as a verb for this) |
| stroke | A reward that the hand gives to the creature. | pet (as a verb) |
| slap | A punishment that the hand gives to the creature. | hit, smack |
| leash | A rope from the hand or a post to the creature. | tether, rope (for the leash itself) |
| alignment | How good or evil a player or a creature is, from -1 to 1. | morality, karma |
| fight | A duel between two creatures in an arena. | battle, combat (as a noun for one fight) |
| arena | The ring where two creatures fight. | ring (for the arena) |

### 1.3 Villagers, villages and buildings

| Word | Meaning | Do not use |
|---|---|---|
| villager | A person who lives in a town. | person (for this), NPC, citizen, peasant |
| disciple | A villager that the player gave one job (farmer, forester, breeder and the others). | worker (for this), follower |
| village | A group of buildings and villagers that worship one player or no player. The game calls it a village. | settlement, hamlet, city |
| town | The code name for a village (`Town`, `TownSystem`). In prose, use "village". | |
| village centre | The centre building of a village. | town hall |
| totem | The pole at the village centre that shows the village's creature. | statue (for this) |
| tribe | The people of a village: Celtic, Norse, Greek, Japanese, Aztec, Indian, Tibetan, African, Egyptian. | race, civilisation |
| child | A villager who is not yet an adult. | kid, baby (except for a newborn) |
| crèche | The building where the children of a village live. | nursery |
| workshop | The building where villagers make scaffolds. | factory |
| scaffold | A frame that villagers make to put up a new building. | blueprint |
| building site | A place where villagers make a new building. | construction site, plot |
| wonder | The large building that the tribe of a village makes for its god. | monument |
| football pitch | The field where villagers play football. | stadium |
| abode | Any building where villagers live or work. | house (for all abodes) |
| storage pit | The building where a town keeps food and wood. | store (for the building), warehouse |
| worship site | The place where villagers dance and pray for a player. | altar (for this), shrine |
| belief | How much a town worships a player. | faith, devotion |
| prayer power | The power that worship gives to a player for miracles. | mana |
| reaction | A thing that villagers, animals or creatures notice and answer. | event (for this), stimulus |
| state | The current activity of a villager, from the state table. | mode, status (for this) |

### 1.4 The temple

| Word | Meaning | Do not use |
|---|---|---|
| temple | The building of a player, with rooms inside. | citadel (except as a code name), castle |
| temple heart | The part of the temple that holds its life. | core, soul |
| temple entrance | The door of the temple from the land. | gate (for this) |
| room | One part of the inside of the temple. | chamber |

### 1.5 Miracles

| Word | Meaning | Do not use |
|---|---|---|
| miracle | A spell that a player casts with prayer power. | spell (for the player's miracle), power |
| cast | To make a miracle start. | fire (as a verb), trigger |
| one-shot globe | A ball on the land that gives one miracle when the hand taps it. | bubble, orb (except as a code name) |
| dispenser | The building that gives a town's miracle to the hand. | shrine, fountain |
| seed | The miracle in the hand before the player casts it. | charge |
| shield | The miracle that makes a dome round a place. | barrier |

### 1.6 Animals and nature

| Word | Meaning | Do not use |
|---|---|---|
| animal | A living thing that is not a villager or a creature. | beast, critter, mob |
| flock | A group of animals that move together. | herd (for all groups), pack |
| grazer | An animal that eats grass (cow, sheep, horse, pig, goat). | herbivore |
| hunter | An animal that hunts other animals or villagers (lion, tiger, wolf, leopard). | predator (except as a code name), carnivore |
| bird | An animal that flies (dove, seagull, bat and the others). | flyer |
| fish | An animal that lives in the sea or in a fish farm. Plural: fish. | fishes |
| fish farm | The place in the sea where villagers catch fish. | fishery, pond |
| shepherd | The disciple who keeps a flock of animals. | herder |
| tree | A plant that grows wood. | plant (for a tree) |
| forest | A group of trees. | wood (for a group of trees), grove |
| field | The land where a farmer grows grain. | farm (for the land), crop (for the land) |
| grain | The food that a field grows. | wheat, crop (for the food) |
| rock | A large stone on the land. | boulder, stone (for a large rock) |
| mushroom | A magic plant that changes how a creature acts when it eats it. | shroom |

### 1.7 Resources

| Word | Meaning | Do not use |
|---|---|---|
| food | The resource that villagers and creatures eat. | provisions, rations |
| wood | The resource that villagers use to make buildings. | lumber, timber, logs (for the resource) |
| pile | A quantity of food or wood that lies on the land. | heap, stack |
| pot | A container that holds a resource on the land. | jar, vessel |
| store | To keep a resource in the storage pit. | stash, stockpile |

### 1.8 The land and the world

| Word | Meaning | Do not use |
|---|---|---|
| land | One map of the game (Land 1, Land 2). Also the ground surface. | level, island (for the map), stage |
| land block | One square of the land's mesh. | tile, chunk |
| map cell | One square of the grid that holds things for searches. | tile, bucket |
| thing | Any object in the world that is not the land. | entity (in prose), object (in prose, except a code name) |
| feature | A fixed thing that is not a building (a rock, a fence, a sign). | prop, decoration |
| scroll | A thing on the land that starts a challenge or gives help. | quest marker |
| challenge | A task that a script gives to the player. | quest (except as a code name), mission |
| script | The land's program that controls the story and challenges. | mission file |
| cut scene | A part of a script that controls the camera and the screen. | cinematic, cutscene |
| cinema bars | The black bars at the top and bottom of the screen in a cut scene. | letterbox |
| vortex | The opening that takes things from one land to the next. | portal, wormhole |
| advisor | The good or the evil spirit that talks to the player. | conscience, helper, spirit (alone) |

### 1.9 Time

| Word | Meaning | Do not use |
|---|---|---|
| turn | One step of the game's logic. There are 10 turns in one second at normal speed. | tick, update (for this) |
| frame | One drawing of the screen. | tick (for this) |
| game time | The time of the game, which stops in a pause and changes with the game speed. | sim time, world time |
| real time | The time of the computer clock. | wall time, system time |

## 2. Words to use and words not to use

Use the word on the left. Do not use the words on the right.

| Use | Meaning | Do not use |
|---|---|---|
| start | To make a thing begin. | begin, commence, kick off, initiate, launch (in prose) |
| stop | To make a thing end. | halt, cease, terminate, kill |
| continue | To do more of the same. | go on, keep on, resume (for this) |
| remove | To take a thing out. | delete (for things in the world), take out, get rid of |
| add | To put more of a thing in. | append (in prose), insert (in prose) |
| make | To cause a thing to exist or to be. | create (in prose), spawn, generate |
| get | To come to have a value. | obtain, fetch, retrieve |
| show | To make a thing visible. | display (as a verb), render (in prose), reveal |
| hide | To make a thing not visible. | conceal |
| use | To do work with a thing. | utilize, leverage, employ |
| find | To get the location or the value of a thing that you do not know. | look up (phrasal verb), locate |
| keep | To not change, or to not remove. | retain, preserve, hold on to |
| change | To make a thing different. | modify, alter, tweak |
| correct | To make a thing right. | fix (as a verb), repair (for code), patch |
| prevent | To make a thing not occur. | avoid, block (as a verb for this) |
| occur | To come to be. | happen, take place, arise |
| necessary | That you must have. | required (as an adjective), needed |
| possible | That can occur. | feasible, viable |
| before | Earlier than. | prior to, previously |
| after | Later than. | following, subsequently |
| near | At a small distance from. | close to, nearby |
| large | Of a big size or quantity. | big, huge, massive |
| small | Of a little size or quantity. | little, tiny |
| fast | At high speed. | quick, rapid |
| slow | At low speed. | sluggish |
| approximately | Nearly, not exactly. | about, around, roughly |
| because | For the reason that. | since (for a reason), as (for a reason), due to |
| if | On the condition that. | in case, provided that |
| also | In addition. | as well, additionally, furthermore |
| only | And no other. | just (for this), merely, solely |
| same | Not different. | identical, equal (for this) |
| different | Not the same. | distinct, dissimilar |

## 3. Technical verbs for code and data

These verbs are technical verbs for this subject. Use each one with the meaning given.

| Verb | Meaning |
|---|---|
| call | To make a function run. |
| return | To give a value back from a function. |
| set | To give a value to a variable or a field. |
| read | To get a value from a variable, a file or a field. |
| write | To put a value into a variable, a file or a field. |
| load | To read a file into memory. |
| save | To write data to a file. |
| draw | To put a thing on the screen. |
| compile | To make object code from source code. |
| match | To give the same bytes as the original game (bw1-decomp). |
| inline | To put the body of a function where the function is called (bw1-decomp). |

## How to add a word

1. Use the existing word if one has the meaning that you need.
2. If no word has that meaning, add a row to the correct table. Give one meaning and the words not to use.
3. Do not give a new meaning to a word that is already in the dictionary.
