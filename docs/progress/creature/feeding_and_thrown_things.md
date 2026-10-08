# Feeding and thrown things

The player feeds the creature by handing it things from the hand, by putting food where it will find it, and by
stroking it while it holds food; it decides for itself what to do with what it is given, is sick on what it can't eat
and gets high on mushrooms. Things thrown at it are caught out of the air when it can reach them in time, or strike it.

How the creature eats what it finds by itself is in [object_actions.md](object_actions.md), what eating does to its body
in [physiology.md](physiology.md), the blow of a thrown thing in [../physics/impact_damage.md](../physics/impact_damage.md),
and thrown people in the air in [../physics/thrown_living.md](../physics/thrown_living.md). The hand's side of giving
(turning the held thing, drawing the hand to the creature) is in [../hand/holding.md](../hand/holding.md).

**Progress: 21/71 done, 12 partial — 38%**

## Giving from the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding something over a creature that would take it, the hand offers it and turns it to face the creature | todo | see [../hand/holding.md](../hand/holding.md) "Giving to the creature"; openblack can't hand things to a creature |
| Only the creature's own player, or a player allied to it, can give it things; another god's creature won't take anything from the hand | todo | nothing gives to creatures in openblack |
| A spell seed is never given; nor can anything be given to a creature under a script's control, or in one state the game excludes (not identified) | todo | |
| Something the creature couldn't pick up isn't given, and the creature's help explains why | todo | the help message's wording was not traced |
| Letting go over such a creature hands the thing over instead of dropping it (the advisors: "put your hand in front of him and then hold the Action Button") | todo | |
| Given something, the creature at once drops what it was doing, turns to the hand looking curious, and reaches to take the thing from it | todo | the reach is the object reach of [object_actions.md](object_actions.md), which openblack has, but nothing starts it from the hand |
| Moving the hand away more than 15 times the creature's size calls the giving off, and the creature stops reaching | todo | see [../hand/holding.md](../hand/holding.md) |
| Every machine in a networked game sees the creature take it | todo | no network play; see [../multiplayer/](../multiplayer/) |

## What it does with what it is given

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If it was already set on doing something that wants a thing of this kind (going off to eat, say), it carries that on with the thing given | todo | |
| A one-off miracle given to a creature grown far enough is cast, aggressively or kindly | todo | see [creature_casting.md](creature_casting.md) |
| Otherwise it weighs four desires, each by how good it has learnt acting on that kind of thing is for it: curiosity looks it over, hunger eats it, wanting to play with the player throws it; compassion (stroking it) is never chosen this way | todo | the eat, look-over and throw actions exist in openblack (`CreatureObjectActionSystem`) but nothing chooses among them for a given thing |
| Hunger only counts for a thing of a kind it hasn't learnt it can't eat | todo | |
| If no desire is at least a tenth strong, the first time it is given a kind of thing it looks it over; after that it does with it what it would when led to it on the leash | todo | see [leash.md](leash.md) |
| Being given something is not by itself a lesson; it learns from the player's stroke or slap afterwards | todo | nothing was found in the giving that teaches it; the hand's row "things taken from the hand teach it the player wanted it to have them" is unconfirmed |

## What it can eat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It may try to eat anything it can pick up, other than toys, spell seeds and other creatures; stores, fields and fish farms it eats from where they stand | partial | openblack only ever tries things with a food value (`CreatureObjectActionSystem::FoodValueOf`); eating stores, fields and fish farms in place is todo ([object_actions.md](object_actions.md)) |
| A thing's food value is its kind's value from the game's tables, but only for kinds marked as meat or plant food; a pot or pile is worth the food in it | partial | `FoodValueOf` takes the table value without the food kind, so special villagers, tortoises and crops count as food there, and leaves animals out entirely |
| The tables: a villager 250, a cow or horse 1200, a sheep 800, a lion or tiger 900, a wolf 700, a pig 290, birds 40 to 50, magic food 16, a mushroom 99, a magic mushroom 50, a toadstool 150; rocks, trees, poo, crops, Egyptian pots and wood are worth nothing | partial | the values come from the game's tables (`InfoConstants`); animals and the food kinds aren't used, see above |
| Special villagers (trainer, priests, monks, breeders, footballers, marauders and others) and tortoises are no food | todo | openblack counts them by their table value |
| Anything worth less than 5 it can't stomach: it is sick, its wish to eat is held back for a while, and it never tries that kind of thing again | todo | openblack never picks such things up to eat (test `FoodItCantPickUpIsGivenUpUneaten` covers only what it can't lift) |
| Poisoned food (people, animals or piles a script poisoned) makes it sick and holds back its wish to eat, but it may try that kind again | todo | `SET_POISONED` and the poison queries are stubs in `src/CHLApi.cpp`; a toadstool counts as poisoned food too, see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Some things the game marks as not to be eaten are spat out and dropped, and it is sick | todo | which marks these are (one is set while a script holds the thing, one during help) was not fully identified |
| Magic mushrooms and toadstools are eaten and make it high for a while, the cause of its stoned look; how many it has eaten is kept | todo | see [physiology.md](physiology.md) "can get high"; the temple scroll has the count's label only (`src/3D/TempleScrolls.cpp`) |
| Eating lowers its hunger by the food's value over a measure of its body (at most 0.8) times its species' factor, never below nothing nor above full | partial | openblack lowers hunger by the eating action's table multiplier (`Satisfied` in `CreatureMindSystem.cpp`); the body measure the game uses was not identified |
| Eating fills it, fattens it and builds up poo | done | see [physiology.md](physiology.md) |
| How many of each kind of thing, and of villagers, animals and mushrooms, it has eaten is counted | todo | |
| A person eaten dies, the death put down to the creature's player, and their town is frightened | partial | removed from home and town and the town frightened (`CreatureObjectActionSystem::Consume`); the death keeps no cause |
| Eating people is evil (the advisors say so) | todo | how the creature's alignment moves was not traced here; see [physiology.md](physiology.md) |
| The creature of one kind of player (likely the computer gods; not identified) never eats its own player's people, unless a script controls it | todo | |

## Force-feeding and lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroking the creature while it holds something makes it eat it ("Stroking the Creature while he's holding an object will make him eat that object") | partial | `LearnFromFeedback` in `CreatureMindLearning.cpp` (the action table's eat-when-stroked mark, food only, from the second stage); how the game ties it to the table was not traced |
| A stroke or slap soon after eating teaches it to eat that sort of thing more or less | done | credit goes to the eating for 20 seconds; see [learning_from_feedback.md](learning_from_feedback.md) |
| It says what it learnt ("From now on, your Creature will eat that sort of thing more"; mushrooms have their own lines) | todo | see [lessons_and_help.md](lessons_and_help.md) |
| Its desire panel lines: hungry from low energy, from watching people eat, from sadness, going to eat something odd, going to eat the weird mushrooms | todo | see [creature_mode.md](creature_mode.md) and [lessons_and_help.md](lessons_and_help.md) |

## Food near it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pot or pile of food raises its kind's reaction when it is made, put down by the hand, lands or stops burning | partial | raised when the hand puts it down (`GameHandGrabWorld::SetUpPotReaction`); not on landing, making or burning out |
| A creature that can see the food takes the reaction up (not the creature of the kind of player above) | todo | openblack's reactions reach creatures (`ReactionSystem`) but a creature does nothing with food |
| Reacting to food, it only comes to know of it, so that its mind may choose to eat it; it doesn't go straight to it | todo | openblack's creature finds food anywhere within reach without seeing it (`NearestFood` in `CreatureMindSystem.cpp`) |
| Food it has found it picks up, looks over and eats, or eats where it lies | done | see [object_actions.md](object_actions.md) |

## Catching things thrown at it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every thing that starts to fly, thrown or knocked, is offered to every creature to catch | partial | Ported (`PhysicsGameHooks::OfferToCatchingCreatures`, `src/Creature/CreatureCatch.*`, `CreatureMindSystem::ForceCatch`, `CreatureObjectActionSystem` catch, test `test_creature_catch`, `6afe6217`); open: R20's start checks (body action, held by a creature), the waiting stand pose and the step's acceptance test, see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| A creature tries only when not fighting or set on a fight, not under a script's control, not already catching, and free to react | done | In the game's order: not held to stroke or slap, not set on a fight, not fighting, not under a script's control, no scripted only-desire or free to react, not already catching; the 0.8 weight test uses the thing's own weight (`creature_catch`, `DynamicsSystemInterface::WeightOf`, `cf273ae0`) |
| The thing must weigh less than eight tenths of the creature and be able to be picked up | todo | |
| It never catches what it threw itself; a throw by a god not allied to its player it catches only three times in a hundred | done | `PhysicsGameHooks::OfferToCatchingCreatures` (own throws refused; another player's only when GameRand(100) ≤ 2; openblack has no alliances) |
| It must be moving across the land at 1 m/s or more and come closest within 5 seconds, late enough for the catch to be started, and pass within the catch's reach | done | `creature_catch` reach test with clip 225's play time and travel and the catch time (`src/Creature/CreatureCatch.*`, test `test_creature_catch`) |
| Catching, it drops what it was doing, stops, smiles and goes to catch it | partial | `CreatureMindSystem::ForceCatch` fails its action and forces a Play/Catch plan; the ready phase waits for a turn, gives up on things behind or too late, side-steps and catches in the arrival window, waiting while its body is busy (`creature_catch::ReadyToCatch`, `PendingCatch`, `cf273ae0`, `6afe6217`); the creature stands breathing as it waits, and a step is refused while it moves or into a blocked cell's circle over the block the game checks (`dc312635`); open: the step check's object circles (the game's route plan per kind of object) |
| It catches with a blend of four catching animations, high or low and left or right, by the thing's height and side, the other hand mirrored | done | Bilinear blend of clips 228–231 clamped to [−0.2, 1.2], the left hand when the thing passes on the left, hand points measured once per creature, last height kept when the thing is gone (`CreatureCatch`, `CatchHands`, `cf273ae0`, `6afe6217`) |
| At the catch's moment, if the thing is still within reach, it is taken out of the air into the creature's hand; otherwise it is missed and flies on | done | Taken out of the physics into the hand (people and animals held) or missed; the clip then runs back as the game's does (`CreatureObjectActionSystem`, `6afe6217`); the catch clip's sounds are heard through the creature's animation sounds, which follow its slot (`CreatureAudioSystem`) |
| A person caught goes into the held state, an animal plays its held animation | todo | see [../physics/thrown_living.md](../physics/thrown_living.md) |
| Food caught isn't eaten as part of the catch; what it does with the thing afterwards is its next ordinary choice | todo | the catch sets nothing to follow (unconfirmed beyond that) |
| The "catch a fireball and throw it back" action is listed but always fails, so creatures never do it | done | the game never does it; openblack neither |
| Creatures watch things flying through the air | todo | see [reactions.md](reactions.md) |

## Being hit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A thing that strikes it and isn't caught hurts it by the blow's crush; toys don't hurt it | done | see [../physics/impact_damage.md](../physics/impact_damage.md) |
| A blow sways its upper or lower body | done | `src/Creature/CreatureSway.*`; see [../physics/impact_damage.md](../physics/impact_damage.md) |
| The player's own creature, while a script controls it, isn't hurt by blows at all | done | `ScriptControlled` from the script object table (`ScriptObjectsSystem`) checked in `HurtCreature` |
| Struck during a fight, it neither reels nor loses fight health or life | partial | openblack sends the blow through the miracle path, so in a duel it reels and loses fight health (`magic_living::TakesEffectItsOwnWay`) |
| Outside a fight the blow can cut or scar its skin where it struck | done | Cut from the striking body's centre toward the groin through the game's skin-mark ray (`src/ECS/CreatureScars.*`, `6afe6217`, test `test_creature_marks`); see [marks.md](marks.md) |
| Its fear and its anger from being damaged rise by the harm | done | `HurtCreature`; for another player's blow openblack also adds them in `MagicLiving.cpp`, which the game's blow path doesn't appear to do (unconfirmed) |
| When the blow is put down to its own player, it thinks less of its player by the harm | done | `HurtCreature` tests the creature's own body's credit, which the game also tests |
| It doesn't turn on the thrower: no plan or reaction is aimed at whoever threw | done | none in either |
| The collision sounds as for any blow, by the two materials | done | see [../physics/impact_damage.md](../physics/impact_damage.md) |
| Its creature learns "damage by throwing at" from the player's throws | done | see [../physics/impact_damage.md](../physics/impact_damage.md) |
| The life lost shows on the status panel, and at no life it faints | done | see [physiology.md](physiology.md) |

## People given or thrown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager given from the hand is taken like anything else and looked over, eaten or thrown by its desires | todo | giving is missing (above) |
| A villager thrown at it is caught if it can be, and then held; otherwise it strikes the creature and is hurt by its own landing | partial | the blow and the landing are done ([../physics/thrown_living.md](../physics/thrown_living.md)); catching is missing |
| Holding a person frightens their town | done | see [object_actions.md](object_actions.md) |

## Other gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Another god can't feed your creature unless allied | todo | see "Giving from the hand" |
| Another god's throws at your creature are caught only three times in a hundred | done | Another player's throw is caught only when GameRand(100) ≤ 2 (`PhysicsGameHooks::OfferToCatchingCreatures`); openblack has no alliances, so every other player counts as unallied |
| Another god's blows hurt, frighten and anger it but don't change its feeling for its own player | partial | done in `HurtCreature`; openblack has no other gods' hands to throw |

## The tutorial and the advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first land's lesson on eating: the creature is starved, the trainer says "Your Creature is getting hungry", "find him something to eat", "some things are better for him than others" | todo | see [../story/land_1.md](../story/land_1.md) "The creature's learning"; every step: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-2-learning-to-eat) |
| While the player looks for food, the advisors comment on what the hand is near: pork (evil), corn from the fields (good), beef (evil, "meat-eaters are more aggressive"), people (evil; children deeply evil), grain from the village store (good) | todo | same script |
| Once the hand holds something near the creature: "Give him food by putting your hand in front of him and then holding the Action Button" | todo | same script |
| The lesson waits until it has eaten once (alive, after looking over, from the hand or from a magic food pile), keeping it wanting nothing but food; then "He seemed to like that", "Try feeding him some more" | todo | same script; see [development_phases.md](development_phases.md) |
| Reminders and help: low energy ("give him something to eat"), "Your Creature is going to eat this", the stage's criteria | todo | see [lessons_and_help.md](lessons_and_help.md) |
| The ogre on the first land can be fed instead of fought, which makes him drowsy | todo | see [../story/land_1.md](../story/land_1.md) "The Ogre" |
