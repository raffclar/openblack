# Picking up

The Action button over a loose thing takes it into the god hand: villagers, animals, rocks, trees, logs, piles of food
and wood, scaffolds, toys and more. The hand holds one thing at a time; a handful of food or wood grows while the button
is held (see [multi_pickup.md](multi_pickup.md)) and trees are pulled free first (see [tug.md](tug.md)).
openblack (`physics` branch): `HandGrabSystem` (`src/ECS/Systems/Implementations/HandGrabSystem.cpp`, rules in
`src/Hand/HandGrabRules.cpp`, the game side in `GameHandGrabWorld.cpp`) picks things up with the game's checks, in its
order. Tests: `test/hand/test_hand_grab.cpp`, `test/hand/test_hand_grab_system.cpp`.

**Progress: 28/35 done, 0 partial — 80%**

## When the hand may take something

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand holds a single thing at a time; with something in it, nothing else can be picked up | done | `hand_grab::PassesGate` (space in the hand) |
| Only things that say they can be picked up, and that the hand is allowed to hold, are taken | done | `hand_grab::ValidForPlaceInHand` per kind; test `HandGrab.AnyFailedCheckMakesThePressATap` |
| Things already in a hand, carried by a tornado, or that a script made unpickable can't be taken | done | `PassesGate`; `SET_ID_PICKUPABLE` (`CHLApi.cpp`); test `HandGrabSystemWithWorld.ThingsOutOfTheInfluenceOrHeldByAScriptAreLeft` |
| The hand only picks things up inside the player's influence, tested at the hand's point on the land | done | `GameHandGrabWorld` (`InfluenceSystem`); same test |
| Things that can't be pulled (big forests, fields, piles, fireballs) and things in flight wait until the press has lasted 225 ms; anything else starts being pulled at once. A press let go within 225 ms is a tap | done | `HandGrabSystem::Press`/`Release`; tests `HandGrab.APressIsTimedByTheClockButNoLongerThanTheTurnsAllow`, `HandGrabSystemWithWorld.AShortPressIsATap`. Tapping is in [clicking_and_activating.md](clicking_and_activating.md) |
| A thing flying through the air can be grabbed out of it (a catch), tested again every frame of the wait | done | tests `HandGrabSystemWithWorld.AThingInFlightIsCaughtOnlyAfterTheWait`, `AFlyingThingThatLandsDuringTheWaitIsTakenAtOnce` |
| The first pick-up triggers its help message (the catch's own message never fires: it is tested after the thing has already left the physics) | todo | openblack has no help system yet |
| Picking something up sends a pick-up reaction around the hand that people and animals respond to | done | `HandGrabSystem::Take` (reaction 16) |
| A burning thing picked up keeps burning and its fire is told it has started moving (in the hand, unless it is a villager) | done | `FireSystem::StartedMoving` from `Take`; see [../physics/fire.md](../physics/fire.md) |
| A firefly hiding exactly where the picked tree or rock stands is destroyed, and the land's firefly reward table gives a one-shot miracle seed there (nothing when the table is all zero) | todo | openblack has no fireflies (a TODO in `HandGrabSystem.cpp`); see [../nature/fireflies.md](../nature/fireflies.md) |

## What can and can't be picked up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers can be picked up, unless at home, already in a hand, or hiding in a building | done | `ValidForPlaceInHand`; test `HandGrab.VillagersAtHomeHeldOrHidingCantBePickedUp` |
| Animals can be picked up only if their species allows it | done | test `HandGrab.AnimalsOnlyWhenTheirKindAllows` |
| Loose mobile objects (pots, logs, branches, toys, balls) can be picked up | done | `ValidForPlaceInHand` |
| Among fixed things only trees can be picked up (not bushes or features) | done | `ValidForPlaceInHand` |
| Rocks can be picked up unless they are wider than 3.6 units | done | test `HandGrab.RocksWiderThanTheHandCanLiftStay` |
| A field can be grabbed only while it has food in it | done | `GameHandGrabWorld::FieldFactsOf`, `HandGrabSystem::Press`; test `test_hand_grab_system` |
| A fish farm can be grabbed only while it has fish in it | todo | openblack has no fish farms; see [multi_pickup.md](multi_pickup.md) |
| A food or wood pile or store can be grabbed only while it has some of what it holds | done | Pile scooping (`HandGrabSystem`, `GameHandGrabWorld`); test `HandGrabSystemWithWorld.APileIsScoopedIntoAHandfulThatGrowsWhileTheButtonIsHeld` |
| A scaffold can be picked up while loose, or while its site hasn't started building and it was put down recently enough | todo | openblack has no scaffolds; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A big forest gives one of its trees, made on the spot, and counts one tree fewer | todo | see [../nature/](../nature/) |
| Another player's fireball can be caught; the player's own slips through | done | `MagicSystem.cpp` (fireball caught into a fireball in the hand); see [../miracles/](../miracles/) |
| A miracle's one-shot bubble goes into the hand as a ready miracle | done | `MagicSystem::GiveSeedToHand`; see [../miracles/](../miracles/) |
| Creatures can't be picked up: the hand takes hold of them to stroke and slap instead | done | `CreatureHandSystem`; see [creature_contact.md](creature_contact.md) |
| Bonfires, rewards and chests, fragments, help spirits, script highlights, shields and vortices can't be picked up | done | `ValidForPlaceInHand` refuses them |
| Buildings can't be picked up | done | `ValidForPlaceInHand` |

## What happens to the thing taken

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager in the hand is marked as held, remembers what it was doing and goes into its in-hand state | done | `HandGrabSystem::Take`, `villager_memory::StorePreviousState`; IN_HAND entry in `LivingActionSystem.cpp` |
| Picking up a villager of the hand's player in its breeding years alarms those around it | done | Reaction 32 in `HandGrabSystem::Take`; test `HandGrabSystemWithWorld.AVillagerOfTheHandsPlayerAlarmsThoseAboutIt` |
| Villagers and animals in the hand switch to their in-hand animation and cry out | done | Cries (`HandGrabSystem::Take`); villagers draw their in-hand clip 355 (`VillagerPose`), animals their kind's held clip (`AnimalSystem::IntoHand`) |
| An animal picked up is taken out of its flock into a flock of its own (the old flock goes when empty) | done | `HandGrabSystem::Take`; scripts holding flocks don't exist in openblack |
| Pulling up a tree plays a creak from the tree's sounds and counts towards the player's alignment | done | `GameHandGrabWorld` uproot; test `HandGrabSystemWithWorld.ALightTreeIsUprootedAndCreaks` |
| A field gives a first handful of 25 (or what it has), halved when the crop is ripe | done | Field scoop (`hand_grab` rules, `FieldSystem` removal); halved when the crop is ripe; test `test_field_crop`, `test_hand_grab_system` |
| A pile gives a handful of what it holds, poisoned if the pile was | done | Handful done, poisoned from a poisoned pile (`b1b75864`); a store's pile goes through the store's own taking; see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| What is picked up leaves the physics (without its landing) and the map's cells, and stops what it was doing | done | `HandGrabSystem::Take` (`DynamicsSystem::RemoveObject` without end of physics) |
| A creature being dragged on the leash by what is picked up is let go | todo | see [../creature/](../creature/) |
| Things the hand holds are kept in the save and restored on load | todo | see [../engine/](../engine/) |
