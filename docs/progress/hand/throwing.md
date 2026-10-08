# Throwing and letting go

What happens when the Action button is let go with something in the hand: a gentle put-down where the hand is, a throw
with the hand's speed, or the handful poured out onto the ground. Where the thing lands then matters: a store, a
building site, a worship site, the creature, or a job for a villager.

openblack (`physics` branch): `HandGrabSystem::LetGo` releases into the game's physics through
`DynamicsSystem::LetGoFromHand` (`src/ECS/Systems/Implementations/DynamicsFromHand.cpp`); slow pots pour
(`GameHandGrabWorld::PourPot`) and a pot can be given straight to a store or pile under the hand. The other targets
(sacrifice, building sites, disciples) are not ported. Tests: `test/hand/test_hand_grab.cpp`,
`test/hand/test_hand_grab_system.cpp`.

The first land teaches throwing with a silver scroll, [Throwing Stones](../story/silver_scrolls/throwing_stones.md).

**Progress: 16/30 done, 2 partial — 57%**

## Putting down or throwing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The thing leaves the hand where it is drawn in the hand, turned as the hand held it and at its held size, only lifted out of the ground | done | `DynamicsSystem::ReleasePose` |
| It takes the hand's spring velocity at the moment of letting go, and no spin; 180 ms later it gets a one-turn twist | done | `HandGrabSystem::LetGo`, `hand_grab::ReleaseSpinTorque`; see [hand_physics.md](hand_physics.md) |
| Let go moving across the land at no more than 2 units a second, it is put down: lowered onto the ground along the slope and lifted clear of anything it overlaps | done | `DynamicsSystem::LetGoFromHand`; test `HandGrab.AThrowIsFastAcrossTheGround` |
| Faster than that it is thrown and flies | done | Same; see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| There is no separate "set down on another thing" rule: a put-down raised onto something stays in the physics and settles there | done | `LetGoFromHand` (raised, so not landed) |
| A tree put down upright (tilted under 0.2 radians) stays where it is put; tilted further it falls | done | `hand_grab::StandsWherePutDown` / `LeavesPhysicsOnLanding`; test `HandGrab.PeopleFencesAndUprightTreesLeaveThePhysicsOnLanding` |
| A person or animal put down on a slope steeper than about 45 degrees falls instead of standing | done | Same rule (land normal y ≥ 0.7) |
| A thing put down over the sea (or anywhere not dry land) falls in rather than standing | done | `hand_grab::StandsWherePutDown`; test `HandGrab.APutDownStandsOnlyOnLandWithoutBeingRaised` |
| A handful of food or wood let go at a speed squared of at most 5 is poured out where the hand is, adding to stores and piles within 3×3 cells or making a pile, lost in water, and the handful is gone | done | `GameHandGrabWorld::PourPot` |
| Faster than that, the handful is thrown as a pot | done | `LetGoFromHand` |
| A thrown villager drops what it was carrying, which flies on as its own thing | todo | `PhysicsClassHooks::DropCarriedResource` is empty |
| Anything thrown or dropped that doesn't land is offered to creatures nearby to catch | partial | Offered through `PhysicsGameHooks::OfferToCatchingCreatures` and caught by `CreatureCatch` (`cf273ae0`, `6afe6217`); open: R20's start checks (body action, held by a creature), the waiting stand pose and the step's acceptance test, see [../creature/](../creature/) |
| Anything thrown or dropped that doesn't land sends a flying-object reaction out; any animal nearer than twice its speed runs from it | done | The reaction is raised with the player and follows the object; animals nearer than twice its speed flee (`AnimalSystem::SetupReactToFlyingObject`); sheltering villagers react only inside the shield (`6afe6217`) |
| The first throw of the player's triggers a help message | todo | No help system yet |
| A put-down toy can teach the creature to play with it | todo | The toy-mimic hook (`PhysicsClassHooks::ConsiderMimickingToyPlay`) is empty; see [../creature/](../creature/) and [../nature/toys.md](../nature/toys.md) |
| A put-down thing can teach the creature to copy what the player did with it | todo | The landing-mimic hook is empty; see [../creature/](../creature/) |
| The game remembers the last thing the hand dropped | done | `HandGrab` (last dropped, set only by a release that went through) |
| When the hand lets go, its pose eases back to the empty hand's over 0.13 seconds | done | The hand's general 0.13 s state blend eases its pose back (`HandAnimation` fade) |

## Where it lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Food or wood let go onto a store adds to it | partial | Handfuls, trees, dead trees, fences, mushrooms and animals pressed onto a storage pit go into it, with the giving's desire, belief and alignment (`ResourceStoreSystem`, tests `test_store_rules`, `test_hand_grab_system`); worship sites and workshops aren't stores yet |
| Wood let go on a building site or scaffold builds with it | todo | openblack has no building sites; see [../building/construction.md](../building/construction.md) and [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A scaffold let go on another scaffold combines them | todo | openblack has no scaffolds; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A tree put down is replanted, joining a forest nearby or starting one | done | Replanted with its forest search, joining a town's scenic forest near a town (`object_physics::EndTree`, `bf1c879d`) |
| A villager let go on a built worship site's altar is sacrificed, for (0.5 × life + 0.5) of its sacrifice value (1.25 times for a child); animals and trees too, by their life | todo | see [../worship/](../worship/) |
| Food let go at a worship site, and wood at a workshop, supplies it | todo | see [../worship/](../worship/) |
| A villager put down near a job (chosen while held from the objects within 3×3 cells, by a 6 m falloff times each object's pull) becomes a disciple doing it: farmer, forester, fisherman, builder, breeder (with a villager of the other sex), trader, missionary, worshipper, craftsman, or moving house | todo | see [../villager/](../villager/) |
| Making a disciple plays the advisor's line for that kind of disciple | todo | see [../audio/](../audio/) |
| A villager put down beside another house of its player moves in; near its own home nothing special happens | todo | see [../villager/](../villager/) |
| Thrown people impress or frighten the villages that watch them fly (the flying-object reaction, reach 25, weighed by each watcher's distance), which also sways the player's alignment | todo | see [../worship/](../worship/) |
| What lands from a throw hurts what it hits and is hurt by its fall | done | See [../physics/impact_damage.md](../physics/impact_damage.md) (`PhysicsGameHooks::ReactToImpact`) |
| Every throw from the hand is credited to the player, whatever its size | done | The thrower's player is carried on the body (`LetGoFromHand`, `PhysicsEntry`) |
