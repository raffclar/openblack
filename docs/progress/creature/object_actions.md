# Object actions

Everything a creature does with the things around it and on the spot: picking things up and holding them, looking them
over, eating and drinking, sleeping, its bodily functions, throwing, knocking things down, and the gestures and
expressions it makes. Each action is chosen by the creature's mind (see [decision_making.md](decision_making.md)) and
played on its body by animations, with the thing taken hold of or let go of at a moment of the animation.

**Progress: 58/123 done, 18 partial — 54%**

## Picking up and holding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature walks up to a thing, stopping short of it by both their sizes, and reaches for it | done | `CreatureObjectActionSystem` (pick up), `CreatureCastMoves`; `CreatureLocomotionSystem::MoveToObject` |
| Reaching blends four reaching animations (front and back, left and right) by where the thing lies | done | `src/Creature/CreatureReach.*`; tests `ACornerTakesAllTheWeight`, `TheMiddleBlendsAllFourEvenly` |
| A thing on the creature's left is reached for with the other hand, the animations mirrored | done | test `TheOtherSideIsReachedMirrored` |
| Something out of reach of the four animations makes it step closer and try again, then give up | done | test `StretchingPastTheCornersExtrapolatesThenFails`, `AnObjectStepWaitsForTheHandsAndGivesUpWhenTheyFail` |
| The thing is taken hold of at the moment of the animation set for the species | done | `CreatureObjectAction` take-hold time from the species' animations |
| What it holds rides in its hand, held by its middle, turned with the hand | done | `CreatureHeldObject` |
| Only light enough things can be picked up; the heavier the creature is the more it can lift | partial | anything mobile, villagers and food pots can be picked up (`CanPickUp`); the weight limit by size is todo (see [physiology.md](physiology.md)); scaffolds have their own pick-up and stealing rules (see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md)) |
| A villager picked up stops what it was doing | done | `CreatureObjectActionSystem` (a villager picked up stops walking) |
| Carrying something heavy makes the creature stronger over time | done | `CreatureObjectActionSystem::ProcessTurn` with the species' carrying rate |
| A young creature that hasn't learnt to pick things up can't, and is told so | todo | see [development_phases.md](development_phases.md) |
| The player can give the creature something by dropping it into its hand, and it takes it | todo | the hand can't hand things to a creature yet; see [../hand](../hand/) and [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Holding something it has no more use for, it puts it down | done | `CreatureIdleMind` (put down when idle) |
| Holding a villager always frightens the creature's town | done | `CreatureObjectActions::AttitudeTo`; test `TheTownFearsThrowingAndEatingVillagers` |

## Looking things over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Examine by picking up: it picks the thing up and turns it over in its hand | done | plan action "examine by picking up"; test `ACuriousCreatureLooksSomethingOver` |
| While holding it, it strokes, shakes, smells or looks closely at it | done | `CreatureObjectActions` keep animations |
| Examine by looking: it goes up to the thing and looks at it for a while | done | plan action "examine by looking" |
| Examine by following: it follows a living thing about for a while | done | plan action "examine by following"; `CreatureLocomotionSystem::Follow` |
| Examine a place: it goes to where something happened and looks round there | partial | it goes to look at where a miracle struck (`CreatureMiracleReactions`); other places todo |
| Inspect another creature: walks up to it and looks it over | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| Look at what it holds in its hand, eat it, stroke it or throw it, as chosen for the thing | partial | the keep animations and eating play; choosing among them by the thing is the idle rule only; see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Look but don't approach, or look at something forever, as scripts and reactions ask | todo | |
| What it learns about a thing it examined counts towards its beliefs about such things | partial | see [beliefs_and_opinions.md](beliefs_and_opinions.md) |

## Eating and drinking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Eat after examining: picks up food, looks it over and eats it | done | plan action and idle need; test `EatingIsPickingUpExaminingAndEating`, `ItPicksFoodUpAndEatsIt` |
| Eat alive: grabs a living thing (villager, animal) and eats it | done | plan action "eat alive" (live food target) |
| Eating a villager removes them from their home and town and frightens the town | done | `CreatureObjectActionSystem::Consume`; test `TheTownFearsThrowingAndEatingVillagers` |
| Each thing eaten is worth the food value the game's tables give its kind | done | `CreatureObjectActionSystem::FoodValueOf` (villagers, animals and things by their tables, pots by the food in them) |
| Eating fills it, fattens it if it overeats, and builds up poo | done | `CreaturePhysiologySystem::Eat`; see [physiology.md](physiology.md) |
| Stomp and eat: stamps on something first, then eats it | todo | |
| Stone and eat: kills something with a thrown stone, then eats it | todo | |
| Eat from the storehouse's food pile where it lies | partial | "eat from food pile" eats pots and piles of food it can pick up; eating from the storehouse in place is todo |
| Eat from a field, pulling up the crops | todo | see [../resources](../resources/) |
| Eat from a tree, shaking fruit down | todo | |
| Eat from a magic food pile | partial | magic food piles count as food pots (unconfirmed they are all eaten the same way) |
| Fish and eat: wades in, scoops up fish and eats them | todo | |
| Food it can't pick up is given up uneaten | done | test `FoodItCantPickUpIsGivenUpUneaten` |
| Poisoned food makes it sick and holds back its hunger for a while | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Drink from the sea: goes to the water's edge, kneels and drinks | done | plan action "drink from the sea"; test `ItDrinksAtTheWatersEdge` |
| Drinking quenches its thirst and it doesn't want water again for a while | done | `CreatureMindSystem` (drink suppression) |
| Eats the grain from fields only when the time is ripe | todo | (unconfirmed what "ripe" means here) |
| Eating gets the creature's own eating sounds | done | from the animations; see [animation.md](animation.md) |

## Sleeping and resting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sleep on the spot: lies down, sleeps with its eyes closed until rested, gets up | done | plan action and idle need; test `ItSleepsWithItsEyesClosedUntilRested` |
| Sleep by something (a tree, its home, a friend) | todo | |
| Sleep at a given place, as scripts and its home ask | todo | |
| Sleeping heals it and rests it, faster asleep than awake | done | `CreaturePhysiologySystem`; see [physiology.md](physiology.md) |
| Rest to get better: lies down when hurt until healthier | partial | after a knock-out it rests until healthy (`CreatureFightSystem`); resting by choice when hurt is todo |
| Rest on the spot for a little while | todo | |
| Sit down for a while, and get up | done | plan action "sit down"; test `SometimesItSitsForAWhile`, `AnAbandonedSitEnds` |
| Faints wherever it is when exhausted, starved or out of life, and comes round later | done | test `FaintedItLiesStillThenComesRound`; see [physiology.md](physiology.md) |
| Woken up, it looks dazed for a moment | todo | |

## Bodily functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Poo: squats and has a poo, which takes about four seconds | done | test `APooTakesFourSecondsAndDropsAsItEnds` |
| The poo is left on the ground as a thing others can see, step in or pick up | todo | openblack empties the creature's bowels but leaves nothing behind |
| Poo discreetly: goes somewhere out of the way first | todo | |
| Some things can be pooed on and others not | todo | |
| Puke: when ill, it is sick | done | plan action "puke" |
| Fart | todo | |
| Sneeze | done | plan action "sneeze" (an emote) |
| Shiver when cold | done | plan action "shiver" |
| Show it is hot: fans itself | done | plan action "show hotness" |
| Scratch an itch | done | plan action "scratch" |
| An evil winner of a fight poos on the loser | done | `CreatureFightSystem` (evil winners) |

## Throwing and letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hurl: picks something up and throws it hard at a target, playing a flat or high throw by the target's height | done | plan action "hurl"; `src/Creature/CreatureThrow.*`; tests `TheReleaseVelocityLandsOnTheTarget`, `HighTargetsBlendInTheHighThrow` |
| An angry creature hurls what it picks up at a home or a tree | done | `CreatureIdleMind` (angry hurl); test `ThrowingAboutAimsFromWhereItStands` |
| Hurl what it already holds in its hand | done | the throw lets go of what is held |
| What is thrown flies until it lands, bounces and comes to rest | done | test `ThingsBounceThenComeToRest`, `FlightTimeIsTheTimeToFallTheDistance` |
| It won't throw at something too close | done | test `ItThrowsAtNothingTooClose` |
| Things thrown hurt what they land on, and villagers thrown are hurt or killed | done | The creature's releases fly in the game's physics and strike with its blows (`LetGoFromHand`, `PhysicsGameHooks::ReactToImpact`) |
| A thrown villager frightens the creature's town | done | test `TheTownFearsThrowingAndEatingVillagers` |
| Throw around: tosses something about playfully nearby | done | plan action "throw around" |
| Throw in the sea: carries something to the shore and throws it in | partial | throws it somewhere nearby, not at the sea (noted in `CreaturePlanActions.cpp`) |
| Throw to impress: throws something far in front of an audience of villagers | partial | throws nearby without an audience |
| Practice throwing | todo | |
| Throw stones into the sea with a friend | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| Throw something at the camera | todo | |
| Throw a ball at something, kick a ball around | partial | both throw what it picks up nearby; balls aren't special yet; the toy actions in full: [../nature/toys.md](../nature/toys.md) |
| Catch something thrown at it and hold it (catching a fireball to throw back always fails in the game, so it never happens) | partial | Ported (`PhysicsGameHooks::OfferToCatchingCreatures`, `src/Creature/CreatureCatch.*`, `CreatureObjectActionSystem` catch, test `test_creature_catch`, `cf273ae0`, `6afe6217`); open: R20's start checks (body action, held by a creature), the waiting stand pose and the step's acceptance test, see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| Put down: sets what it holds down gently | done | `CreatureObjectActions` put down |
| Discard: tosses what it holds away, with some of the hand's swing | done | test `TossingKeepsSomeOfTheHandsSpeed` |
| Lob gently | done | `CreatureThrow` gentle lob |
| Its throw can go wrong while it is unskilled: the thing goes astray | todo | see [lessons_and_help.md](lessons_and_help.md) |

## Knocking things down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walks up and strikes with the four striking animations blended | done | `CreatureReach` strike animations |
| Stomp on something | partial | plan action "stomp": the thing is removed at the blow, abodes take damage; no crushing or squash |
| Kick something | partial | as stomping |
| Kick a tree | partial | the tree is removed, not felled; see [../nature](../nature/) |
| A home struck is damaged by the creature's size | done | `CreatureObjectActionSystem` (physical damage to abodes) |
| Things it stamps on are crushed: villagers and animals killed, small things broken | todo | |
| Break a rock, smash stones in half | todo | see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Destroy whoever attacked it | todo | |
| A struck thing's town fears the creature | done | `AttitudeTo` |

## Fire and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Start a fire, or set something on fire | todo | see [../physics](../physics/) |
| Put out a fire by stamping on it | todo | |
| Put out a fire with the water miracle | todo | see [creature_casting.md](creature_casting.md) |
| Put out a fire on itself, by rolling or running to water | todo | |

## Gestures and expressions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Point at something, low or high, to its left or right, turning to it first | done | `CreatureObjectActions` point |
| Point at the camera, at the hand, or at something off-screen to show the player | partial | pointing at the camera plays; pointing at the hand or off-screen things is todo |
| Wave at the player | done | plan action "wave at player" |
| Wave at something | todo | |
| Look at the hand, look at the camera | done | plan actions |
| Look at the camera in wide screen, during cut scenes | todo | |
| Pull silly faces | done | plan action "pull silly faces" |
| Show an impressive animation | done | plan action "show impressive animation" |
| Be sad, be frightened on the spot | done | plan actions |
| Look confused | todo | |
| Be pathetic to the player, to get attention | done | plan action "be pathetic to player" |
| Be cross with the player | todo | |
| Show the player how nice it thinks they are | todo | |
| Howl at the player | done | plan action "howl at player" |
| Pray to the player | todo | |
| Look at its reflection in the water | todo | |
| Watch the telly (unconfirmed what it shows) | todo | |
| Behave strangely, get high (from magic mushrooms and toadstools) | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Communicate its state: show the player its strongest desire | done | plan action "communicate state"; test `ItShowsItsStrongestDesireOnceAMinute` |
| Show a lesson it has learnt, by playing the action | todo | see [lessons_and_help.md](lessons_and_help.md) |
| Draw a shape in the air for a gesture (circle, star, spiral, heart and others) | todo | the gestures before casting are in [creature_casting.md](creature_casting.md) |

## Going places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Go to the top of a hill and look round, sit on it, or walk along a ridge | todo | |
| Explore the coast, explore the towns | todo | see [idle_behaviour.md](idle_behaviour.md) |
| Run around a race track | todo | |
| Go to the middle of the screen, to where the player is looking | todo | |
| Go to the hand | partial | the leash leads it to the hand ([leash.md](leash.md)); going to the hand by itself is todo |
| Run to something | partial | it runs when the plan asks for it (`CreatureLocomotionSystem`) |
| Run away from something, from a place, from the player | done | plan actions "run away from object" and "run away from player"; `FleeFrom` |
| Go to a teleport and use it | partial | creatures jump between teleport stones (`TeleportSystem`); choosing to go to one is todo |
| Sit down on a beach | todo | |
| Enter the citadel | todo | see [home_and_pen.md](home_and_pen.md) |
